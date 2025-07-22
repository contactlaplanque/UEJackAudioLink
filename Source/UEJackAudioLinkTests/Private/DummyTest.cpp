#include "Misc/AutomationTest.h"

//  Category  : UEJackAudioLink
//  SubCat    : Dummy
//  Test name : UEJackAudioLink.Dummy
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUEJackAudioLink_DummyTest, "UEJackAudioLink.Dummy", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter);

bool FUEJackAudioLink_DummyTest::RunTest(const FString& Parameters)
{
    TestTrue(TEXT("Dummy always passes"), true);
    return true;
}