#include <obs-module.h>
#include <media-io/audio-io.h>
#include <memory>
#include <cstring>
#include <string>
#include <vector>
#include "Calibration.h"
#include "VoiceChain.h"

void gv_open_calibration(obs_source_t* src, std::shared_ptr<Capture> cap, double sampleRate);
std::vector<std::string> gv_duplicates(obs_source_t* filter, bool disable);
void gv_auto_sync(obs_source_t* filter, int latencyMs);
bool gv_ask_text(const char* titleKey, const char* labelKey, const char* initial, std::string& out);

#define S_GAIN    "gain_db"
#define S_BYPASS  "bypass"
#define S_DENOISE "denoise"
#define S_ENGINE  "engine"
#define S_CLEAN   "m_clean"
#define S_WARMTH  "m_warmth"
#define S_CLARITY "m_clarity"
#define S_PUNCH   "m_punch"
#define S_DEST    "destination"
#define S_LATENCY "latency_info"
#define S_CALIB   "calibrate"
#define S_DUPS    "duplicates"
#define S_DUPS_OFF "duplicates_off"
#define S_SYNC    "auto_sync"
#define S_PRESET  "preset"         // nom du preset actif ; le changer (UI ou obs-websocket) charge le preset
#define S_PRESETS "presets"        // tableau {name, values}
#define S_PRESET_SAVE "preset_save"
#define S_PRESET_DEL  "preset_delete"

// Ce qu'un preset retient (tout le son, sauf Avant/Apres)
static const char* kPresetKeys[] = {"gain_db", "denoise", "engine", "m_clean", "m_warmth", "m_clarity", "m_punch",
                                    "destination", "corr_low", "corr_presence", "corr_air"};

struct VoiceFilter {
    obs_source_t* context = nullptr;
    VoiceChain chain;
    std::shared_ptr<Capture> capture = std::make_shared<Capture>(); // partage avec la fenetre de calibration
    size_t channels = 2;
    std::string loadedPreset; // dernier preset applique : on ne recharge que si le nom change
};

// Copie les valeurs d'un preset dans les reglages. false si introuvable.
static bool preset_load(obs_data_t* settings, const char* name)
{
    obs_data_array_t* arr = obs_data_get_array(settings, S_PRESETS);
    bool found = false;
    for (size_t i = 0; arr && i < obs_data_array_count(arr) && !found; ++i) {
        obs_data_t* p = obs_data_array_item(arr, i);
        if (std::strcmp(obs_data_get_string(p, "name"), name) == 0) {
            obs_data_t* v = obs_data_get_obj(p, "values");
            for (const char* k : kPresetKeys)
                if (obs_data_has_user_value(v, k)) {
                    obs_data_item_t* it = obs_data_item_byname(v, k);
                    switch (obs_data_item_numtype(it)) {
                    case OBS_DATA_NUM_INT: obs_data_set_int(settings, k, obs_data_item_get_int(it)); break;
                    case OBS_DATA_NUM_DOUBLE: obs_data_set_double(settings, k, obs_data_item_get_double(it)); break;
                    default: obs_data_set_bool(settings, k, obs_data_item_get_bool(it)); break;
                    }
                    obs_data_item_release(&it);
                }
            obs_data_release(v);
            found = true;
        }
        obs_data_release(p);
    }
    obs_data_array_release(arr);
    return found;
}

// Enregistre les reglages courants sous 'name' (remplace s'il existe)
static void preset_save(obs_data_t* settings, const char* name)
{
    obs_data_t* v = obs_data_create();
    for (const char* k : kPresetKeys) {
        obs_data_item_t* it = obs_data_item_byname(settings, k);
        if (!it) continue;
        if (obs_data_item_gettype(it) == OBS_DATA_BOOLEAN)
            obs_data_set_bool(v, k, obs_data_item_get_bool(it));
        else if (obs_data_item_numtype(it) == OBS_DATA_NUM_INT)
            obs_data_set_int(v, k, obs_data_item_get_int(it));
        else
            obs_data_set_double(v, k, obs_data_item_get_double(it));
        obs_data_item_release(&it);
    }
    obs_data_array_t* arr = obs_data_get_array(settings, S_PRESETS);
    if (!arr) arr = obs_data_array_create();
    size_t i = 0;
    for (; i < obs_data_array_count(arr); ++i) {
        obs_data_t* p = obs_data_array_item(arr, i);
        const bool same = std::strcmp(obs_data_get_string(p, "name"), name) == 0;
        if (same) obs_data_set_obj(p, "values", v);
        obs_data_release(p);
        if (same) break;
    }
    if (i == obs_data_array_count(arr)) {
        obs_data_t* p = obs_data_create();
        obs_data_set_string(p, "name", name);
        obs_data_set_obj(p, "values", v);
        obs_data_array_push_back(arr, p);
        obs_data_release(p);
    }
    obs_data_set_array(settings, S_PRESETS, arr);
    obs_data_array_release(arr);
    obs_data_release(v);
}

static void preset_delete(obs_data_t* settings, const char* name)
{
    obs_data_array_t* arr = obs_data_get_array(settings, S_PRESETS);
    for (size_t i = 0; arr && i < obs_data_array_count(arr); ++i) {
        obs_data_t* p = obs_data_array_item(arr, i);
        const bool same = std::strcmp(obs_data_get_string(p, "name"), name) == 0;
        obs_data_release(p);
        if (same) { obs_data_array_erase(arr, i); break; }
    }
    obs_data_array_release(arr);
}

static const char* vf_get_name(void*)
{
    return obs_module_text("FilterName");
}

static void vf_update(void* data, obs_data_t* settings)
{
    auto* vf = static_cast<VoiceFilter*>(data);
    // Preset change (liste du panneau, ou Streamer.bot / Stream Deck via obs-websocket SetSourceFilterSettings)
    const std::string preset = obs_data_get_string(settings, S_PRESET);
    if (preset != vf->loadedPreset) {
        if (!preset.empty() && !preset_load(settings, preset.c_str()))
            blog(LOG_WARNING, "[gragarda-voice] preset introuvable : %s", preset.c_str());
        else if (!preset.empty())
            blog(LOG_INFO, "[gragarda-voice] preset charge : %s", preset.c_str());
        vf->loadedPreset = preset;
    }
    vf->chain.setGainDb((float)obs_data_get_double(settings, S_GAIN));
    vf->chain.setBypass(obs_data_get_bool(settings, S_BYPASS));
    vf->chain.setDenoise(obs_data_get_bool(settings, S_DENOISE));
    vf->chain.setEngine((VoiceChain::Engine)obs_data_get_int(settings, S_ENGINE));
    VoiceChain::Macros m;
    m.clean = (float)obs_data_get_int(settings, S_CLEAN) / 100.0f;
    m.warmth = (float)obs_data_get_int(settings, S_WARMTH) / 100.0f;
    m.clarity = (float)obs_data_get_int(settings, S_CLARITY) / 100.0f;
    m.punch = (float)obs_data_get_int(settings, S_PUNCH) / 100.0f;
    m.dest = (VoiceChain::Destination)obs_data_get_int(settings, S_DEST);
    m.corrLow = (float)obs_data_get_double(settings, "corr_low"); // poses par la calibration, pas d'UI
    m.corrPresence = (float)obs_data_get_double(settings, "corr_presence");
    m.corrAir = (float)obs_data_get_double(settings, "corr_air");
    vf->chain.setMacros(m);
}

static void* vf_create(obs_data_t* settings, obs_source_t* source)
{
    auto* vf = new VoiceFilter();
    vf->context = source;

    audio_t* audio = obs_get_audio();
    vf->channels = audio_output_get_channels(audio);
    const double sr = (double)audio_output_get_sample_rate(audio);
    char* model = obs_module_file("models/DeepFilterNet3_onnx.tar.gz");
    vf->chain.prepare(sr, (int)vf->channels, model);
    vf->capture->prepare((size_t)(16 * sr));
    if (!vf->chain.maxAvailable())
        blog(LOG_WARNING, "[gragarda-voice] modele DeepFilterNet introuvable (%s) : mode Qualite max inactif",
             model ? model : "?");
    bfree(model);

    vf_update(vf, settings);
    return vf;
}

static void vf_destroy(void* data)
{
    delete static_cast<VoiceFilter*>(data);
}

static struct obs_audio_data* vf_filter_audio(void* data, struct obs_audio_data* audio)
{
    auto* vf = static_cast<VoiceFilter*>(data);
    float* ch[MAX_AV_PLANES] = {};
    for (size_t c = 0; c < vf->channels && c < MAX_AV_PLANES; ++c)
        ch[c] = reinterpret_cast<float*>(audio->data[c]);

    vf->capture->push(ch, (int)vf->channels, audio->frames); // signal brut, avant la chaine
    vf->chain.process(ch, (int)vf->channels, audio->frames);
    return audio;
}

static void vf_defaults(obs_data_t* settings)
{
    obs_data_set_default_double(settings, S_GAIN, 0.0);
    obs_data_set_default_bool(settings, S_BYPASS, false);
    obs_data_set_default_bool(settings, S_DENOISE, true);
    obs_data_set_default_int(settings, S_ENGINE, VoiceChain::Max);
    for (const char* k : {S_CLEAN, S_WARMTH, S_CLARITY, S_PUNCH})
        obs_data_set_default_int(settings, k, 50);
    obs_data_set_default_int(settings, S_DEST, VoiceChain::Stream);
}

static int latency_ms(const VoiceFilter* vf, VoiceChain::Engine e)
{
    return vf ? (int)(1000.0 * vf->chain.latencySamples(e) / vf->chain.sampleRate() + 0.5) : 0;
}

static std::string latency_text(const VoiceFilter* vf, VoiceChain::Engine e)
{
    return std::string(obs_module_text("LatencyInfo")) + " " + std::to_string(latency_ms(vf, e)) + " ms";
}

static VoiceChain::Engine current_engine(const VoiceFilter* vf)
{
    obs_data_t* st = obs_source_get_settings(vf->context);
    const auto e = (VoiceChain::Engine)obs_data_get_int(st, S_ENGINE);
    obs_data_release(st);
    return e;
}

static bool vf_engine_modified(void* data, obs_properties_t* props, obs_property_t*, obs_data_t* settings)
{
    const auto e = (VoiceChain::Engine)obs_data_get_int(settings, S_ENGINE);
    obs_property_set_description(obs_properties_get(props, S_LATENCY),
                                 latency_text(static_cast<VoiceFilter*>(data), e).c_str());
    return true;
}

static obs_properties_t* vf_properties(void* data)
{
    auto* vf = static_cast<VoiceFilter*>(data);
    obs_properties_t* props = obs_properties_create();

    // Filtres OBS en double sur la meme source (compresseur, suppression de bruit...) : avertir + desactiver
    if (vf) {
        const auto dups = gv_duplicates(vf->context, false);
        if (!dups.empty()) {
            std::string names;
            for (auto& n : dups) names += (names.empty() ? "" : ", ") + n;
            obs_property_t* w = obs_properties_add_text(
                props, S_DUPS, (std::string(obs_module_text("DupWarning")) + " " + names).c_str(), OBS_TEXT_INFO);
            obs_property_text_set_info_type(w, OBS_TEXT_INFO_WARNING);
            obs_properties_add_button2(props, S_DUPS_OFF, obs_module_text("DupDisable"),
                [](obs_properties_t*, obs_property_t*, void* d) {
                    auto* f = static_cast<VoiceFilter*>(d);
                    gv_duplicates(f->context, true);
                    obs_source_update_properties(f->context); // l'avertissement disparait
                    return false;
                }, vf);
        }
    }

    // Presets : liste + enregistrer / supprimer
    obs_property_t* pl = obs_properties_add_list(props, S_PRESET, obs_module_text("Preset"), OBS_COMBO_TYPE_LIST,
                                                 OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(pl, obs_module_text("PresetNone"), "");
    if (vf) {
        obs_data_t* st = obs_source_get_settings(vf->context);
        obs_data_array_t* arr = obs_data_get_array(st, S_PRESETS);
        for (size_t i = 0; arr && i < obs_data_array_count(arr); ++i) {
            obs_data_t* p = obs_data_array_item(arr, i);
            obs_property_list_add_string(pl, obs_data_get_string(p, "name"), obs_data_get_string(p, "name"));
            obs_data_release(p);
        }
        obs_data_array_release(arr);
        obs_data_release(st);
    }
    obs_property_set_long_description(pl, obs_module_text("PresetTip"));
    obs_property_set_modified_callback2(pl, [](void* d, obs_properties_t*, obs_property_t*, obs_data_t* st) {
        // charge tout de suite dans le panneau pour que les curseurs suivent
        auto* f = static_cast<VoiceFilter*>(d);
        const char* name = obs_data_get_string(st, S_PRESET);
        if (f && *name && f->loadedPreset != name) preset_load(st, name);
        return true;
    }, vf);
    obs_properties_add_button2(props, S_PRESET_SAVE, obs_module_text("PresetSave"),
        [](obs_properties_t*, obs_property_t*, void* d) {
            auto* f = static_cast<VoiceFilter*>(d);
            obs_data_t* st = obs_source_get_settings(f->context);
            std::string name;
            if (gv_ask_text("PresetSave", "PresetName", obs_data_get_string(st, S_PRESET), name)) {
                preset_save(st, name.c_str());
                obs_data_set_string(st, S_PRESET, name.c_str());
                f->loadedPreset = name; // deja applique : ce sont les reglages courants
                obs_source_update(f->context, st);
                obs_source_update_properties(f->context);
            }
            obs_data_release(st);
            return false;
        }, vf);
    obs_properties_add_button2(props, S_PRESET_DEL, obs_module_text("PresetDelete"),
        [](obs_properties_t*, obs_property_t*, void* d) {
            auto* f = static_cast<VoiceFilter*>(d);
            obs_data_t* st = obs_source_get_settings(f->context);
            const std::string name = obs_data_get_string(st, S_PRESET);
            if (!name.empty()) {
                preset_delete(st, name.c_str());
                obs_data_set_string(st, S_PRESET, "");
                f->loadedPreset.clear();
                obs_source_update(f->context, st);
                obs_source_update_properties(f->context);
            }
            obs_data_release(st);
            return false;
        }, vf);

    obs_property_t* g = obs_properties_add_float_slider(
        props, S_GAIN, obs_module_text("Gain"), -24.0, 24.0, 0.1);
    obs_property_float_set_suffix(g, " dB");

    obs_properties_add_button2(props, S_CALIB, obs_module_text("Calibrate"),
        [](obs_properties_t*, obs_property_t*, void* d) {
            auto* f = static_cast<VoiceFilter*>(d);
            if (f)
                gv_open_calibration(f->context, f->capture, f->chain.sampleRate());
            return false;
        }, vf);

    // 4 macros : 50 = reglage de reference
    const char* macros[][2] = {{S_CLEAN, "Clean"}, {S_WARMTH, "Warmth"}, {S_CLARITY, "Clarity"}, {S_PUNCH, "Punch"}};
    for (auto& mk : macros) {
        obs_property_t* p = obs_properties_add_int_slider(props, mk[0], obs_module_text(mk[1]), 0, 100, 1);
        obs_property_set_long_description(p, obs_module_text((std::string(mk[1]) + "Tip").c_str()));
    }
    obs_property_t* dest = obs_properties_add_list(props, S_DEST, obs_module_text("Destination"),
                                                   OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(dest, obs_module_text("DestStream"), VoiceChain::Stream);
    obs_property_list_add_int(dest, obs_module_text("DestPodcast"), VoiceChain::Podcast);
    obs_property_list_add_int(dest, obs_module_text("DestVoiceOver"), VoiceChain::VoiceOver);

    obs_properties_add_bool(props, S_DENOISE, obs_module_text("Denoise"));
    obs_property_t* eng = obs_properties_add_list(props, S_ENGINE, obs_module_text("Engine"),
                                                  OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(eng, obs_module_text("EngineMax"), VoiceChain::Max);
    obs_property_list_add_int(eng, obs_module_text("EngineFast"), VoiceChain::Fast);
    obs_property_set_modified_callback2(eng, vf_engine_modified, vf);

    obs_properties_add_bool(props, S_BYPASS, obs_module_text("Bypass"));

    // Latence a compenser sur la camera + synchro automatique (apres confirmation)
    const auto e = vf ? current_engine(vf) : VoiceChain::Max;
    obs_property_t* info = obs_properties_add_text(props, S_LATENCY, latency_text(vf, e).c_str(), OBS_TEXT_INFO);
    obs_property_text_set_info_type(info, OBS_TEXT_INFO_NORMAL);
    obs_properties_add_button2(props, S_SYNC, obs_module_text("AutoSync"),
        [](obs_properties_t*, obs_property_t*, void* d) {
            auto* f = static_cast<VoiceFilter*>(d);
            if (f)
                gv_auto_sync(f->context, latency_ms(f, current_engine(f)));
            return false;
        }, vf);

    return props;
}

struct obs_source_info gragarda_voice_filter_info = {};

void gragarda_voice_register_info()
{
    auto& i = gragarda_voice_filter_info;
    i.id             = "gragarda_voice_filter";
    i.type           = OBS_SOURCE_TYPE_FILTER;
    i.output_flags   = OBS_SOURCE_AUDIO;
    i.get_name       = vf_get_name;
    i.create         = vf_create;
    i.destroy        = vf_destroy;
    i.update         = vf_update;
    i.filter_audio   = vf_filter_audio;
    i.get_defaults   = vf_defaults;
    i.get_properties = vf_properties;
}
