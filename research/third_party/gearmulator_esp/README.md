# gearmulator's ESP core

An unmodified copy of four files from The Usual Suspects' [gearmulator](https://github.com/dsp56300/gearmulator), at commit `fad253d3c3390f40f4653bc0f01003cf176c509c`,
`source/ronaldo/custom_chips/esp/`: `esp_common.h`, `esp_runtime.h`, `esp_base.h`, `esp_interpreter.h`. They are the interpreter of JE-8086's Roland/Toshiba ESP (TC170C140AF), GPL-3.0, the same licence as this project.

Only `xt_fx` compiles them: they're the reference `plugin/Source/XtFxChip.*` is checked against (`XT_FX_MAIN`). They aren't in the plugin. Don't edit them: to take a later version, copy
the files again, change the commit above and see what `xt_fx` says.
