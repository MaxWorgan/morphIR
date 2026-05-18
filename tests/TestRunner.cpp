#include <juce_core/juce_core.h>

int main()
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure(false);
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        auto* result = runner.getResult(i);
        if (result->failures > 0)
        {
            juce::Logger::writeToLog("FAILED: " + result->unitTestName
                + " / " + result->subcategoryName);
            ++failures;
        }
    }

    return failures > 0 ? 1 : 0;
}
