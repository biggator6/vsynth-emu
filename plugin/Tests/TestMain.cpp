#include "TestMain.h"
#include "TestAppDir.h"
#include <juce_events/juce_events.h>
#include <cstdio>

int testMain(int argc, char** argv, const char* name, const TestRunWith& run) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s rom.2.bin\n", argc > 0 ? argv[0] : "");
        return 1;
    }
    juce::ScopedJuceInitialiser_GUI init;
    TestAppDir appDir;
    const juce::File work = appDir.dir.getChildFile(name);
    work.createDirectory();
    juce::StringArray rest;
    for (int i = 2; i < argc; ++i) rest.add(argv[i]);
    return run(juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]), work, rest);
}

int testMain(int argc, char** argv, const char* name, TestRun run) {
    return testMain(argc, argv, name, [run](const juce::File& rom, const juce::File& work, const juce::StringArray&) {
        return run(rom, work);
    });
}
