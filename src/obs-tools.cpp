// Outils cote OBS : detection des filtres en double, synchronisation automatique (thread UI).
#include <obs-module.h>
#include <QDialog>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QLabel>
#include <QMessageBox>
#include <QScreen>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <algorithm>
#include <cctype>
#include <QInputDialog>
#include <QString>
#include <cstring>
#include <string>
#include <vector>

static QString T(const char* key) { return QString::fromUtf8(obs_module_text(key)); }

static const char* kFilterId = "gragarda_voice_filter";
static const char* kSyncName = "Gragarda Voice — sync";

// Filtres OBS qui refont un travail deja fait par Gragarda Voice
static bool isDuplicate(const char* id)
{
    static const char* ids[] = {"noise_suppress_filter", "noise_suppress_filter_v2", "nvidia_audiofx_filter",
                                "compressor_filter", "limiter_filter", "expander_filter",
                                "upward_compressor_filter", "basic_eq_filter"};
    for (const char* d : ids)
        if (id && std::strcmp(id, d) == 0) return true;
    return false;
}

// VST : on ne sait pas ce qu'il fait, mais son nom le dit presque toujours (MCompressor, ReaFIR, MEqualizer...).
// ponytail: devinette par mots-cles ; un VST au nom exotique passe entre les mailles
static bool isDuplicateVst(obs_source_t* f)
{
    if (std::strcmp(obs_source_get_unversioned_id(f), "vst_filter") != 0) return false;
    obs_data_t* st = obs_source_get_settings(f);
    std::string path = obs_data_get_string(st, "plugin_path");
    obs_data_release(st);
    const size_t slash = path.find_last_of("/\\");
    std::string file = slash == std::string::npos ? path : path.substr(slash + 1);
    for (char& ch : file) ch = (char)std::tolower((unsigned char)ch);
    static const char* keys[] = {"compress", "limit", "equaliz", "eq.", "eq_", "reaeq", "reafir", "denois", "noise",
                                 "rnnoise", "deess", "de-ess", "sibil", "gate", "expand", "dynamic", "leveler",
                                 "loudness", "maxim", "voice", "vocal", "deverb", "declick"};
    for (const char* k : keys)
        if (file.find(k) != std::string::npos) return true;
    return false;
}

// Noms des doublons actifs sur la source du filtre ; disable = les desactiver
std::vector<std::string> gv_duplicates(obs_source_t* filter, bool disable)
{
    std::vector<std::string> out;
    obs_source_t* parent = obs_filter_get_parent(filter);
    if (!parent) return out;
    struct Ctx { std::vector<std::string>* out; bool disable; } ctx{&out, disable};
    obs_source_enum_filters(parent, [](obs_source_t*, obs_source_t* f, void* p) {
        auto* c = static_cast<Ctx*>(p);
        if (!obs_source_enabled(f) ||
            (!isDuplicate(obs_source_get_unversioned_id(f)) && !isDuplicate(obs_source_get_id(f)) &&
             !isDuplicateVst(f)))
            return;
        c->out->push_back(obs_source_get_name(f));
        if (c->disable) obs_source_set_enabled(f, false);
    }, &ctx);
    return out;
}

static bool hasOurFilter(obs_source_t* src)
{
    bool found = false;
    obs_source_enum_filters(src, [](obs_source_t*, obs_source_t* f, void* p) {
        if (std::strcmp(obs_source_get_id(f), kFilterId) == 0) *static_cast<bool*>(p) = true;
    }, &found);
    return found;
}

// Synchronisation : cameras (video asynchrone) -> filtre "Retard de rendu" ; autres micros -> decalage audio.
// Pas l'audio du bureau / des applis : le jeu (video non retardee) resterait synchro avec son propre son.
void gv_auto_sync(obs_source_t* filter, int latencyMs)
{
    struct Plan { obs_source_t* src; bool video; bool audio; std::string note; };
    struct Ctx { obs_source_t* self; int ms; std::vector<Plan> plan; } ctx{obs_filter_get_parent(filter), latencyMs, {}};

    obs_enum_sources([](void* p, obs_source_t* s) {
        auto* c = static_cast<Ctx*>(p);
        if (s == c->self || hasOurFilter(s)) return true;
        const uint32_t flags = obs_source_get_output_flags(s);
        const char* id = obs_source_get_unversioned_id(s);
        const bool camera = (flags & OBS_SOURCE_ASYNC_VIDEO) && std::strcmp(id, "ffmpeg_source") != 0 &&
                            std::strcmp(id, "vlc_source") != 0;
        const bool mic = std::strcmp(id, "wasapi_input_capture") == 0;
        if (!camera && !mic) return true;
        Plan pl{s, camera, (flags & OBS_SOURCE_AUDIO) != 0, ""};
        const int64_t cur = obs_source_get_sync_offset(s) / 1000000;
        if (pl.audio && cur != 0 && cur != c->ms) {
            pl.audio = false;
            pl.note = T("SyncKeepOffset").arg(cur).toStdString();
        }
        c->plan.push_back(pl);
        return true;
    }, &ctx);

    if (ctx.plan.empty()) {
        QMessageBox::information(nullptr, T("SyncTitle"), T("SyncNothing"));
        return;
    }
    QString list;
    for (auto& p : ctx.plan) {
        list += "<li><b>" + QString::fromUtf8(obs_source_get_name(p.src)) + "</b> : ";
        QStringList what;
        if (p.video) what << T("SyncVideo").arg(latencyMs);
        if (p.audio) what << T("SyncAudio").arg(latencyMs);
        if (!p.note.empty()) what << QString::fromStdString(p.note);
        list += what.join(", ") + "</li>";
    }
    // Liste dans une zone defilante : avec beaucoup de sources la fenetre reste a l'ecran, boutons visibles
    QDialog box;
    box.setWindowTitle(T("SyncTitle"));
    auto* lay = new QVBoxLayout(&box);
    auto* intro = new QLabel(T("SyncConfirm").arg(latencyMs));
    intro->setWordWrap(true);
    auto* view = new QTextBrowser;
    view->setHtml("<ul>" + list + "</ul>");
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Yes | QDialogButtonBox::Cancel);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &box, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &box, &QDialog::reject);
    lay->addWidget(intro);
    lay->addWidget(view, 1);
    lay->addWidget(buttons);
    const QRect scr = QGuiApplication::primaryScreen()->availableGeometry();
    box.resize(std::min(700, scr.width() * 8 / 10), std::min(600, scr.height() * 7 / 10));
    if (box.exec() != QDialog::Accepted) return;

    for (auto& p : ctx.plan) {
        if (p.video) {
            obs_source_t* f = obs_source_get_filter_by_name(p.src, kSyncName);
            obs_data_t* st = obs_data_create();
            obs_data_set_int(st, "delay_ms", latencyMs);
            if (f) {
                obs_source_update(f, st);
            } else {
                f = obs_source_create_private("gpu_delay", kSyncName, st);
                obs_source_filter_add(p.src, f);
            }
            obs_data_release(st);
            obs_source_release(f);
        }
        if (p.audio)
            obs_source_set_sync_offset(p.src, (int64_t)latencyMs * 1000000);
        blog(LOG_INFO, "[gragarda-voice] sync : %s (video %d, audio %d) -> %d ms", obs_source_get_name(p.src), p.video,
             p.audio, latencyMs);
    }
}

// Saisie d'un nom (presets). false si annule ou vide.
bool gv_ask_text(const char* titleKey, const char* labelKey, const char* initial, std::string& out)
{
    bool ok = false;
    const QString s = QInputDialog::getText(nullptr, T(titleKey), T(labelKey), QLineEdit::Normal,
                                            QString::fromUtf8(initial), &ok).trimmed();
    if (!ok || s.isEmpty()) return false;
    out = s.toStdString();
    return true;
}
