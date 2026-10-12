#include <obs-module.h>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("gragarda-voice", "en-US") // anglais par defaut ; fr-FR.ini charge si OBS est en francais

extern struct obs_source_info gragarda_voice_filter_info;
void gragarda_voice_register_info();

bool obs_module_load(void)
{
    gragarda_voice_register_info();
    obs_register_source(&gragarda_voice_filter_info);
    blog(LOG_INFO, "[gragarda-voice] charge (v0.4.0-alpha.3)");
    return true;
}

void obs_module_unload(void)
{
    blog(LOG_INFO, "[gragarda-voice] decharge");
}
