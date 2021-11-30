#include "ModConfig.hpp"
#include "UI.hpp"

#include "beatsaber-hook/shared/utils/il2cpp-utils.hpp"
#include "beatsaber-hook/shared/utils/utils.h"
#include "beatsaber-hook/shared/utils/hooking.hpp"

#include "questui/shared/QuestUI.hpp"
#include "questui/shared/BeatSaberUI.hpp"
#include "config-utils/shared/config-utils.hpp"
#include "custom-types/shared/register.hpp"

#include "TMPro/TextMeshProUGUI.hpp"

#include "HMUI/ViewController_AnimationType.hpp"

#include "System/DateTime.hpp"

#include "UnityEngine/Sprite.hpp"
#include "UnityEngine/Resources.hpp"
#include "UnityEngine/Color.hpp"
#include "UnityEngine/Vector4.hpp"
#include "UnityEngine/Sprites/DataUtility.hpp"
#include "UnityEngine/UIVertex.hpp"
#include "UnityEngine/UI/VertexHelper.hpp"
#include "UnityEngine/Mathf.hpp"

#include "GlobalNamespace/SoloFreePlayFlowCoordinator.hpp"
#include "GlobalNamespace/PartyFreePlayFlowCoordinator.hpp"
#include "GlobalNamespace/LevelCompletionResults.hpp"
#include "GlobalNamespace/LevelCompletionResultsHelper.hpp"
#include "GlobalNamespace/IBeatmapLevel.hpp"
#include "GlobalNamespace/BeatmapDifficulty.hpp"
#include "GlobalNamespace/BeatmapData.hpp"
#include "GlobalNamespace/GameplayModifiers.hpp"
#include "GlobalNamespace/SinglePlayerLevelSelectionFlowCoordinator.hpp"
#include "GlobalNamespace/PlatformLeaderboardViewController.hpp"
#include "GlobalNamespace/LeaderboardsModel.hpp"
#include "GlobalNamespace/LocalLeaderboardViewController.hpp"
#include "GlobalNamespace/PauseMenuManager.hpp"
#include "GlobalNamespace/NoteCutInfo.hpp"
#include "GlobalNamespace/GameNoteController.hpp"
#include "GlobalNamespace/SaberSwingRatingCounter.hpp"
#include "GlobalNamespace/SaberSwingRating.hpp"
#include "GlobalNamespace/SaberMovementData.hpp"
#include "GlobalNamespace/ScoreModel.hpp"
#include "GlobalNamespace/ScoreController.hpp"
#include "GlobalNamespace/BladeMovementDataElement.hpp"
#include "GlobalNamespace/NoteData.hpp"
#include "GlobalNamespace/AudioTimeSyncController.hpp"

#include "unordered_map"

using namespace GlobalNamespace;

DEFINE_CONFIG(ModConfig);

static BSDUI::LevelStats* levelStatsView = nullptr;
static BSDUI::ScoreGraph* scoreGraphView = nullptr;
static std::unordered_map<SaberSwingRatingCounter*, NoteCutInfo> swingMap;
static UnityEngine::UI::Button* detailsButton;
static float realRightSaberDistance;

ModInfo modInfo;
IDifficultyBeatmap* lastBeatmap;
Tracker tracker;
std::vector<std::pair<float, float>> percents;
SinglePlayerLevelSelectionFlowCoordinator* levelSelectCoordinator;

Logger& getLogger() {
    static Logger* logger = new Logger(modInfo);
    return *logger;
}

static UnityEngine::Color32 colorToc32(UnityEngine::Color color) {
    return UnityEngine::Color32(color.r * 255, color.g * 255, color.b * 255, color.a * 255);
}
static UnityEngine::Vector2 v3tov2(UnityEngine::Vector3 v) {
    return UnityEngine::Vector2(v.x, v.y);
}

void disableDetailsButton() {
    if(detailsButton)
        detailsButton->get_gameObject()->set_active(false);
}

// Hooks
// function to avoid duplicate code
void processResults(SinglePlayerLevelSelectionFlowCoordinator* self, LevelCompletionResults* levelCompletionResults, IDifficultyBeatmap* difficultyBeatmap, bool practice) {
    if(!levelStatsView)
        levelStatsView = QuestUI::BeatSaberUI::CreateViewController<BSDUI::LevelStats*>();
    if(!scoreGraphView)
        scoreGraphView = QuestUI::BeatSaberUI::CreateViewController<BSDUI::ScoreGraph*>();
    
    // exit through pause or something
    if(levelCompletionResults->levelEndStateType == LevelCompletionResults::LevelEndStateType::None)
        return;

    if(!getModConfig().ShowPass.GetValue() && levelCompletionResults->levelEndStateType == LevelCompletionResults::LevelEndStateType::Cleared)
        return;
    
    if(!getModConfig().ShowFail.GetValue() && levelCompletionResults->levelEndStateType == LevelCompletionResults::LevelEndStateType::Failed)
        return;

    self->SetLeftScreenViewController(levelStatsView, HMUI::ViewController::AnimationType::None);
    
    if(getModConfig().ShowGraph.GetValue())
        self->SetRightScreenViewController(scoreGraphView, HMUI::ViewController::AnimationType::None);

    // add completion results to tracker
    tracker.notes = levelCompletionResults->goodCutsCount + levelCompletionResults->badCutsCount + levelCompletionResults->missedCount;
    tracker.score = levelCompletionResults->rawScore;
    tracker.l_distance = levelCompletionResults->leftSaberMovementDistance;
    tracker.r_distance = realRightSaberDistance;
    tracker.misses = levelCompletionResults->missedCount;
    tracker.combo = levelCompletionResults->maxCombo;

    auto time = System::DateTime::get_Now().ToLocalTime();
    tracker.date = to_utf8(csstrtostr(time.ToString(il2cpp_utils::createcsstr("dddd, d MMMM yyyy h:mm tt"))));

    levelStatsView->setText(difficultyBeatmap);
    
    // don't save on practice or fails
    if(getModConfig().Save.GetValue() && !practice && levelCompletionResults->levelEndStateType != LevelCompletionResults::LevelEndStateType::Failed) {
        addDataToFile(difficultyBeatmap);
        if(detailsButton)
            detailsButton->get_gameObject()->set_active(true);
    }
}

MAKE_HOOK_MATCH(ProcessResultsSolo, &SoloFreePlayFlowCoordinator::ProcessLevelCompletionResultsAfterLevelDidFinish, void, SoloFreePlayFlowCoordinator* self, LevelCompletionResults* levelCompletionResults, IDifficultyBeatmap* difficultyBeatmap, GameplayModifiers* gameplayModifiers, bool practice) {
    ProcessResultsSolo(self, levelCompletionResults, difficultyBeatmap, gameplayModifiers, practice);
    processResults(self, levelCompletionResults, difficultyBeatmap, practice);
}

MAKE_HOOK_MATCH(ProcessResultsParty, &PartyFreePlayFlowCoordinator::ProcessLevelCompletionResultsAfterLevelDidFinish, void, PartyFreePlayFlowCoordinator* self, LevelCompletionResults* levelCompletionResults, IDifficultyBeatmap* difficultyBeatmap, GameplayModifiers* gameplayModifiers, bool practice) {
    ProcessResultsParty(self, levelCompletionResults, difficultyBeatmap, gameplayModifiers, practice);
    // don't show if the enter name view will be displayed
    if(self->WillScoreGoToLeaderboard(levelCompletionResults, LeaderboardsModel::GetLeaderboardID(difficultyBeatmap), practice)) {
        lastBeatmap = difficultyBeatmap;
    } else {
        processResults(self, levelCompletionResults, difficultyBeatmap, practice);
    }
}

MAKE_HOOK_MATCH(PostNameResultsParty, &PartyFreePlayFlowCoordinator::ProcessScore, void, PartyFreePlayFlowCoordinator* self, LevelCompletionResults* levelCompletionResults, Il2CppString* leaderboardId, Il2CppString* playerName) {
    PostNameResultsParty(self, levelCompletionResults, leaderboardId, playerName);
    processResults(self, levelCompletionResults, lastBeatmap, true); // might be failed instead of practice but same effect
}

// another function for solo/party overlap
bool flashed = false;
void primeLevelStats(LeaderboardViewController* self, IDifficultyBeatmap* difficultyBeatmap) {
    if(!levelStatsView) {
        levelStatsView = QuestUI::BeatSaberUI::CreateViewController<BSDUI::LevelStats*>();
    }
    levelStatsView->get_gameObject()->set_active(false);

    // if someone could please give me a better solution for the back button, it would be much appreciated
    if(!flashed) {
        levelStatsView->get_transform()->set_localScale({0, 0, 0});
        levelSelectCoordinator->SetRightScreenViewController(levelStatsView, HMUI::ViewController::AnimationType::None);
        flashed = true;
    }
    
    if(!detailsButton) {
        getLogger().info("Creating details button");
        detailsButton = QuestUI::BeatSaberUI::CreateUIButton(self->get_transform(), "...", [](){
            // getLogger().info("Displaying level stats from file");
            if(!lastBeatmap)
                return;
            levelSelectCoordinator->SetRightScreenViewController(levelStatsView, HMUI::ViewController::AnimationType::In);
            getDataFromFile(lastBeatmap);
            levelStatsView->setText(lastBeatmap, false);
        });
        reinterpret_cast<UnityEngine::RectTransform*>(detailsButton->get_transform())->set_sizeDelta({10, 10});
        detailsButton->GetComponentInChildren<TMPro::TextMeshProUGUI*>()->set_margin({-14.5, 0, 0, 0});
    }
    lastBeatmap = difficultyBeatmap;
}

MAKE_HOOK_MATCH(LevelLeaderboardSolo, &PlatformLeaderboardViewController::SetData, void, PlatformLeaderboardViewController* self, IDifficultyBeatmap* difficultyBeatmap) {
    LevelLeaderboardSolo(self, difficultyBeatmap);

    auto flowCoordinatorArr = UnityEngine::Resources::FindObjectsOfTypeAll<SoloFreePlayFlowCoordinator*>();
    if(flowCoordinatorArr->Length() < 1)
        return;
    levelSelectCoordinator = flowCoordinatorArr->get(0);

    primeLevelStats(self, difficultyBeatmap);

    // ui is a war
    detailsButton->get_transform()->SetParent(self->get_transform(), false);
    auto rect = reinterpret_cast<UnityEngine::RectTransform*>(detailsButton->get_transform());
    rect->set_anchorMin({0.07, 0.1});
    rect->set_anchorMax({0.07, 0.1});
    detailsButton->get_gameObject()->set_active(isInFile(difficultyBeatmap));
}

MAKE_HOOK_MATCH(LevelLeaderboardParty, &LocalLeaderboardViewController::SetData, void, LocalLeaderboardViewController* self, IDifficultyBeatmap* difficultyBeatmap) {
    LevelLeaderboardParty(self, difficultyBeatmap);

    auto flowCoordinatorArr = UnityEngine::Resources::FindObjectsOfTypeAll<PartyFreePlayFlowCoordinator*>();
    if(flowCoordinatorArr->Length() < 1)
        return;
    levelSelectCoordinator = flowCoordinatorArr->get(0);

    primeLevelStats(self, difficultyBeatmap);

    // ui is a war
    detailsButton->get_transform()->SetParent(self->get_transform(), false);
    auto rect = reinterpret_cast<UnityEngine::RectTransform*>(detailsButton->get_transform());
    rect->set_anchorMin({0.07, 0.85});
    rect->set_anchorMax({0.07, 0.85});
    detailsButton->get_gameObject()->set_active(isInFile(difficultyBeatmap));
}

MAKE_HOOK_MATCH(SongStart, &AudioTimeSyncController::Start, void, AudioTimeSyncController* self) {
    SongStart(self);

    tracker.song_time = self->get_songLength();
    // getLogger().info("Song length: %.2f", tracker.song_time);
}

MAKE_HOOK_MATCH(LevelPlay, &SinglePlayerLevelSelectionFlowCoordinator::StartLevel, void, SinglePlayerLevelSelectionFlowCoordinator* self, System::Action* beforeSceneSwitchCallback, bool practice) {
    LevelPlay(self, beforeSceneSwitchCallback, practice);
    // getLogger().info("Level started");
    tracker = {0};
    tracker.min_pct = 1;
    // idk, a few less resizes I guess
    percents.clear();
    percents.reserve(self->get_selectedDifficultyBeatmap()->get_beatmapData()->cuttableNotesCount);
    swingMap.clear();
    // disable score view in level select if needed
    levelSelectCoordinator->SetRightScreenViewController(levelSelectCoordinator->get_leaderboardViewController(), HMUI::ViewController::AnimationType::In);
    levelStatsView->get_gameObject()->set_active(false);
}

MAKE_HOOK_MATCH(LevelPause, &PauseMenuManager::ShowMenu, void, PauseMenuManager* self) {
    LevelPause(self);
    tracker.pauses++;
}

MAKE_HOOK_MATCH(NoteCut, &ScoreController::HandleNoteWasCut, void, ScoreController* self, NoteController* noteController, ByRef<NoteCutInfo> noteCutInfo) {
    NoteCut(self, noteController, noteCutInfo);
    
    auto cutInfo = noteCutInfo.heldRef;

    // track bad cuts
    if(!cutInfo.get_allIsOK() && noteController->noteData->colorType != -1) {
        float time = self->audioTimeSyncController->get_songTime();

        tracker.notes++;
        float maxScore = calculateMaxScore(tracker.notes);
        float pct = tracker.score / maxScore;
        
        if(pct > 0 && pct < tracker.min_pct)
            tracker.min_pct = pct;
        if(pct < 1 && pct > tracker.max_pct)
            tracker.max_pct = pct;
        // getLogger().info("Score:%i, notes: %i, percent: %.2f, time: %.2f", self->baseRawScore, tracker.notes, pct, time);

        percents.push_back(std::make_pair(time, pct));

        return;
    }

    // add cutInfo to swing map
    SaberSwingRatingCounter* swing = reinterpret_cast<SaberSwingRatingCounter*>(cutInfo.swingRatingCounter);
    if(!swing)
        return;
    
    swingMap[swing] = cutInfo;
    
    // track before cut ratings
    float beforeCutRating = swing->beforeCutRating;
    if(cutInfo.saberType == SaberType::SaberA)
        tracker.l_preSwing += beforeCutRating;
    else
        tracker.r_preSwing += beforeCutRating;
    
    if(beforeCutRating > 1)
        swing->beforeCutRating = 1;
}

MAKE_HOOK_MATCH(NoteMiss, &ScoreController::HandleNoteWasMissed, void, ScoreController* self, NoteController* noteController) {
    NoteMiss(self, noteController);

    if(noteController->noteData->colorType == -1)
        return;
        
    float time = self->audioTimeSyncController->get_songTime();

    tracker.notes++;
    float maxScore = calculateMaxScore(tracker.notes);
    float pct = tracker.score / maxScore;

    if(pct > 0 && pct < tracker.min_pct)
        tracker.min_pct = pct;
    if(pct < 1 && pct > tracker.max_pct)
        tracker.max_pct = pct;
    // getLogger().info("Score:%i, notes: %i, percent: %.2f, time: %.2f", self->baseRawScore, tracker.notes, pct, time);

    percents.push_back(std::make_pair(time, pct));
}

MAKE_HOOK_MATCH(AddScore, &ScoreController::HandleCutScoreBufferDidFinish, void, ScoreController* self, CutScoreBuffer* cutScoreBuffer) {    
    int cutScore = cutScoreBuffer->get_scoreWithMultiplier();
    
    AddScore(self, cutScoreBuffer);
    
    // float time = reinterpret_cast<SaberSwingRatingCounter*>(cutScoreBuffer->saberSwingRatingCounter)->cutTime;
    float time = self->audioTimeSyncController->get_songTime();

    tracker.notes++;
    // I think that using self->baseRawScore might cause issues if two cuts happen too close to each other
    tracker.score += cutScore;
    float maxScore = calculateMaxScore(tracker.notes);
    float pct = tracker.score / maxScore;

    if(pct > 0 && pct < tracker.min_pct)
        tracker.min_pct = pct;
    if(pct < 1 && pct > tracker.max_pct)
        tracker.max_pct = pct;
    // getLogger().info("Score:%i, notes: %i, percent: %.2f, time: %.2f", self->baseRawScore, tracker.notes, pct, time);

    percents.push_back(std::make_pair(time, pct));
}

MAKE_HOOK_MATCH(AngleData, &SaberSwingRatingCounter::ProcessNewData, void, SaberSwingRatingCounter* self, BladeMovementDataElement newData, BladeMovementDataElement prevData, bool prevDataAreValid) {
    bool alreadyCut = self->notePlaneWasCut;
    
    AngleData(self, newData, prevData, prevDataAreValid);

    if(swingMap.count(self) == 0)
        return;
    
    auto cutInfo = swingMap[self];
    bool leftSaber = cutInfo.saberType == SaberType::SaberA;

    float num = UnityEngine::Vector3::Angle(newData.segmentNormal, self->cutPlaneNormal);
    
    if(!alreadyCut) {
        float postAngle = UnityEngine::Vector3::Angle(self->cutTopPos - self->cutBottomPos, self->afterCutTopPos - self->afterCutBottomPos);
        if(leftSaber)
            tracker.l_postSwing += SaberSwingRating::AfterCutStepRating(postAngle, 0);
        else
            tracker.r_postSwing += SaberSwingRating::AfterCutStepRating(postAngle, 0);
    } else {
        if(leftSaber)
            tracker.l_postSwing += SaberSwingRating::AfterCutStepRating(newData.segmentAngle, num);
        else
            tracker.r_postSwing += SaberSwingRating::AfterCutStepRating(newData.segmentAngle, num);
    }

    // avoid resetting the beforeCutRating until after the note has been cut so that the NoteCut hook can add it
    // however, if the note finishes without ever cutting the plane, it still needs to be processed
    if(!self->notePlaneWasCut && !self->finished)
        return;

    // run only when finishing
    if(self->finished) {
        int before, after, accuracy;
        ScoreModel::RawScoreWithoutMultiplier(reinterpret_cast<ISaberSwingRatingCounter*>(self), cutInfo.cutDistanceToCenter, byref(before), byref(after), byref(accuracy));

        if(leftSaber) {
            // getLogger().info("Left swing finishing");
            tracker.l_notes++;
            tracker.l_cut += before + after + accuracy;
            tracker.l_beforeCut += before;
            tracker.l_afterCut += after;
            tracker.l_accuracy += accuracy;
            tracker.l_speed += cutInfo.saberSpeed;
        } else {
            // getLogger().info("Right swing finishing");
            tracker.r_notes++;
            tracker.r_cut += before + after + accuracy;
            tracker.r_beforeCut += before;
            tracker.r_afterCut += after;
            tracker.r_accuracy += accuracy;
            tracker.r_speed += cutInfo.saberSpeed;
        }
        swingMap.erase(self);
    }
    // correct for change in ComputeSwingRating
    if(self->beforeCutRating > 1)
        self->beforeCutRating = 1;
}

// honestly why is this so complicated
// override #1
MAKE_HOOK_MATCH(PreSwingCalc, static_cast<float (SaberMovementData::*)(bool, float)>(&SaberMovementData::ComputeSwingRating), float, SaberMovementData* self, bool overrideSegmenAngle, float overrideValue) {
    if (self->validCount < 2)
        return 0;

    int num = self->data->Length();
    int num2 = self->nextAddIndex - 1;
    if (num2 < 0)
        num2 += num;

    float time = self->data->get(num2).time;
    float num3 = time;
    float num4 = 0;
    UnityEngine::Vector3 segmentNormal = self->data->get(num2).segmentNormal;
    float angleDiff = (overrideSegmenAngle ? overrideValue : self->data->get(num2).segmentAngle);
    int num5 = 2;

    num4 += SaberSwingRating::BeforeCutStepRating(angleDiff, 0);

    while (time - num3 < 0.4 && num5 < self->validCount) {
        num2--;
        if (num2 < 0)
            num2 += num;
        UnityEngine::Vector3 segmentNormal2 = self->data->get(num2).segmentNormal;
        angleDiff = self->data->get(num2).segmentAngle;
        float num6 = UnityEngine::Vector3::Angle(segmentNormal2, segmentNormal);
        if (num6 > 90)
            break;
        num4 += SaberSwingRating::BeforeCutStepRating(angleDiff, num6);
        num3 = self->data->get(num2).time;
        num5++;
    }
    return num4;
}

// fix your game
MAKE_HOOK_MATCH(MakeCompletionResults, &LevelCompletionResultsHelper::Create, LevelCompletionResults*, int levelNotesCount, ::Array<BeatmapObjectExecutionRating*>* beatmapObjectExecutionRatings, GameplayModifiers* gameplayModifiers, GameplayModifiersModelSO* gameplayModifiersModel, int rawScore, int modifiedScore, int maxCombo, ::Array<float>* saberActivityValues, float leftSaberMovementDistance, float rightSaberMovementDistance, ::Array<float>* handActivityValues, float leftHandMovementDistance, float rightHandMovementDistance, float songDuration, LevelCompletionResults::LevelEndStateType levelEndStateType, LevelCompletionResults::LevelEndAction levelEndAction, float energy, float songTime) {
    realRightSaberDistance = rightSaberMovementDistance;
    return MakeCompletionResults(levelNotesCount, beatmapObjectExecutionRatings, gameplayModifiers, gameplayModifiersModel, rawScore, modifiedScore, maxCombo, saberActivityValues, leftSaberMovementDistance, rightSaberMovementDistance, handActivityValues, leftHandMovementDistance, rightHandMovementDistance, songDuration, levelEndStateType, levelEndAction, energy, songTime);
}

// mfers can't even fill an image on a curved surface
// override #2
static void customAddQuad(UnityEngine::UI::VertexHelper* vertexHelper, ::Array<UnityEngine::Vector3>* quadPositions, UnityEngine::Color32 color, ::Array<UnityEngine::Vector3>* quadUVs, float curvedUIRadius) {
    int currentVertCount = vertexHelper->get_currentVertCount();
    UnityEngine::Vector2 uv = UnityEngine::Vector2(curvedUIRadius, 0);
    for (int i = 0; i < 4; i++) {
        vertexHelper->AddVert(quadPositions->get(i), color, v3tov2(quadUVs->get(i)), HMUI::ImageView::_get_kVec2Zero(), uv, HMUI::ImageView::_get_kVec2Zero(), HMUI::ImageView::_get_kVec3Zero(), HMUI::ImageView::_get_kVec4Zero());
    }
    vertexHelper->AddTriangle(currentVertCount, currentVertCount + 1, currentVertCount + 2);
    vertexHelper->AddTriangle(currentVertCount + 2, currentVertCount + 3, currentVertCount);
}
MAKE_HOOK_MATCH(FillImage, &HMUI::ImageView::GenerateFilledSprite, void, HMUI::ImageView* self, UnityEngine::UI::VertexHelper* toFill, bool preserveAspect, float curvedUIRadius) {
    toFill->Clear();
    if (self->get_fillAmount() < 0.001) {
        return;
    }
    UnityEngine::Vector4 drawingDimensions = self->GetDrawingDimensions(preserveAspect);
    UnityEngine::Vector4 obj = (self->get_overrideSprite() ? UnityEngine::Sprites::DataUtility::GetOuterUV(self->get_overrideSprite()) : UnityEngine::Vector4::get_zero());
    UnityEngine::UIVertex simpleVert = UnityEngine::UIVertex::_get_simpleVert();
    simpleVert.color = colorToc32(self->get_color());
    float num = obj.x;
    float num2 = obj.y;
    float num3 = obj.z;
    float num4 = obj.w;
    if (self->get_fillMethod() == UnityEngine::UI::Image::FillMethod::Horizontal || self->get_fillMethod() == UnityEngine::UI::Image::FillMethod::Vertical) {
        if (self->get_fillMethod() == UnityEngine::UI::Image::FillMethod::Horizontal) {
            float num5 = (num3 - num) * self->get_fillAmount();
            if (self->get_fillOrigin() == 1) {
                drawingDimensions.x = drawingDimensions.z - (drawingDimensions.z - drawingDimensions.x) * self->get_fillAmount();
                num = num3 - num5;
            } else {
                drawingDimensions.z = drawingDimensions.x + (drawingDimensions.z - drawingDimensions.x) * self->get_fillAmount();
                num3 = num + num5;
            }
        } else if (self->get_fillMethod() == UnityEngine::UI::Image::FillMethod::Vertical) {
            float num6 = (num4 - num2) * self->get_fillAmount();
            if (self->get_fillOrigin() == 1) {
                drawingDimensions.y = drawingDimensions.w - (drawingDimensions.w - drawingDimensions.y) * self->get_fillAmount();
                num2 = num4 - num6;
            } else {
                drawingDimensions.w = drawingDimensions.y + (drawingDimensions.w - drawingDimensions.y) * self->get_fillAmount();
                num4 = num2 + num6;
            }
        }
    }
    self->_get_s_Xy()->get(0) = UnityEngine::Vector3(drawingDimensions.x, drawingDimensions.y, 0);
    self->_get_s_Xy()->get(1) = UnityEngine::Vector3(drawingDimensions.x, drawingDimensions.w, 0);
    self->_get_s_Xy()->get(2) = UnityEngine::Vector3(drawingDimensions.z, drawingDimensions.w, 0);
    self->_get_s_Xy()->get(3) = UnityEngine::Vector3(drawingDimensions.z, drawingDimensions.y, 0);
    self->_get_s_Uv()->get(0) = UnityEngine::Vector3(num, num2, 0);
    self->_get_s_Uv()->get(1) = UnityEngine::Vector3(num, num4, 0);
    self->_get_s_Uv()->get(2) = UnityEngine::Vector3(num3, num4, 0);
    self->_get_s_Uv()->get(3) = UnityEngine::Vector3(num3, num2, 0);
    if (self->get_fillAmount() < 1 && self->get_fillMethod() != 0 && self->get_fillMethod() != UnityEngine::UI::Image::FillMethod::Vertical) {
        if (self->get_fillMethod() == UnityEngine::UI::Image::FillMethod::Radial90) {
            if (HMUI::ImageView::RadialCut(self->_get_s_Xy(), self->_get_s_Uv(), self->get_fillAmount(), self->get_fillClockwise(), self->get_fillOrigin())) {
                customAddQuad(toFill, self->_get_s_Xy(), colorToc32(self->get_color()), self->_get_s_Uv(), curvedUIRadius);
            }
        } else if (self->get_fillMethod() == UnityEngine::UI::Image::FillMethod::Radial180) {
            for (int i = 0; i < 2; i++) {
                int num7 = ((self->get_fillOrigin() > 1) ? 1 : 0);
                float t;
                float t2;
                float t3;
                float t4;
                if (self->get_fillOrigin() == 0 || self->get_fillOrigin() == 2) {
                    t = 0;
                    t2 = 1;
                    if (i == num7) {
                        t3 = 0;
                        t4 = 0.5;
                    } else {
                        t3 = 0.5;
                        t4 = 1;
                    }
                } else {
                    t3 = 0;
                    t4 = 1;
                    if (i == num7) {
                        t = 0.5;
                        t2 = 1;
                    } else {
                        t = 0;
                        t2 = 0.5;
                    }
                }
                self->_get_s_Xy()->get(0).x = UnityEngine::Mathf::Lerp(drawingDimensions.x, drawingDimensions.z, t3);
                self->_get_s_Xy()->get(1).x = self->_get_s_Xy()->get(0).x;
                self->_get_s_Xy()->get(2).x = UnityEngine::Mathf::Lerp(drawingDimensions.x, drawingDimensions.z, t4);
                self->_get_s_Xy()->get(3).x = self->_get_s_Xy()->get(2).x;
                self->_get_s_Xy()->get(0).y = UnityEngine::Mathf::Lerp(drawingDimensions.y, drawingDimensions.w, t);
                self->_get_s_Xy()->get(1).y = UnityEngine::Mathf::Lerp(drawingDimensions.y, drawingDimensions.w, t2);
                self->_get_s_Xy()->get(2).y = self->_get_s_Xy()->get(1).y;
                self->_get_s_Xy()->get(3).y = self->_get_s_Xy()->get(0).y;
                self->_get_s_Uv()->get(0).x = UnityEngine::Mathf::Lerp(num, num3, t3);
                self->_get_s_Uv()->get(1).x = self->_get_s_Uv()->get(0).x;
                self->_get_s_Uv()->get(2).x = UnityEngine::Mathf::Lerp(num, num3, t4);
                self->_get_s_Uv()->get(3).x = self->_get_s_Uv()->get(2).x;
                self->_get_s_Uv()->get(0).y = UnityEngine::Mathf::Lerp(num2, num4, t);
                self->_get_s_Uv()->get(1).y = UnityEngine::Mathf::Lerp(num2, num4, t2);
                self->_get_s_Uv()->get(2).y = self->_get_s_Uv()->get(1).y;
                self->_get_s_Uv()->get(3).y = self->_get_s_Uv()->get(0).y;
                float value = (self->get_fillClockwise() ? (self->get_fillAmount() * 2 - (float)i) : (self->get_fillAmount() * 2 - (float)(1 - i)));
                if (HMUI::ImageView::RadialCut(self->_get_s_Xy(), self->_get_s_Uv(), UnityEngine::Mathf::Clamp01(value), self->get_fillClockwise(), (i + self->get_fillOrigin() + 3) % 4)) {
                    customAddQuad(toFill, self->_get_s_Xy(), colorToc32(self->get_color()), self->_get_s_Uv(), curvedUIRadius);
                }
            }
        } else {
            if (self->get_fillMethod() != HMUI::ImageView::FillMethod::Radial360)
                return;
            for (int j = 0; j < 4; j++) {
                float t5;
                float t6;
                if (j < 2) {
                    t5 = 0;
                    t6 = 0.5;
                } else {
                    t5 = 0.5;
                    t6 = 1;
                }
                float t7;
                float t8;
                if (j == 0 || j == 3) {
                    t7 = 0;
                    t8 = 0.5;
                } else {
                    t7 = 0.5;
                    t8 = 1;
                }
                // calculate uvs and point borders of quarter
                self->_get_s_Xy()->get(0).x = UnityEngine::Mathf::Lerp(drawingDimensions.x, drawingDimensions.z, t5);
                self->_get_s_Xy()->get(1).x = self->_get_s_Xy()->get(0).x;
                self->_get_s_Xy()->get(2).x = UnityEngine::Mathf::Lerp(drawingDimensions.x, drawingDimensions.z, t6);
                self->_get_s_Xy()->get(3).x = self->_get_s_Xy()->get(2).x;
                self->_get_s_Xy()->get(0).y = UnityEngine::Mathf::Lerp(drawingDimensions.y, drawingDimensions.w, t7);
                self->_get_s_Xy()->get(1).y = UnityEngine::Mathf::Lerp(drawingDimensions.y, drawingDimensions.w, t8);
                self->_get_s_Xy()->get(2).y = self->_get_s_Xy()->get(1).y;
                self->_get_s_Xy()->get(3).y = self->_get_s_Xy()->get(0).y;
                self->_get_s_Uv()->get(0).x = UnityEngine::Mathf::Lerp(num, num3, t5);
                self->_get_s_Uv()->get(1).x = self->_get_s_Uv()->get(0).x;
                self->_get_s_Uv()->get(2).x = UnityEngine::Mathf::Lerp(num, num3, t6);
                self->_get_s_Uv()->get(3).x = self->_get_s_Uv()->get(2).x;
                self->_get_s_Uv()->get(0).y = UnityEngine::Mathf::Lerp(num2, num4, t7);
                self->_get_s_Uv()->get(1).y = UnityEngine::Mathf::Lerp(num2, num4, t8);
                self->_get_s_Uv()->get(2).y = self->_get_s_Uv()->get(1).y;
                self->_get_s_Uv()->get(3).y = self->_get_s_Uv()->get(0).y;
                // find angle to fill in the quarter
                float value2 = (self->get_fillClockwise() ? (self->get_fillAmount() * 4 - (float)((j + self->get_fillOrigin()) % 4)) : (self->get_fillAmount() * 4 - (float)(3 - (j + self->get_fillOrigin()) % 4)));
                // check that the quarter is filled in at all, and cut the dimensions to how they should be
                if (HMUI::ImageView::RadialCut(self->_get_s_Xy(), self->_get_s_Uv(), UnityEngine::Mathf::Clamp01(value2), self->get_fillClockwise(), (j + 2) % 4)) {
                    customAddQuad(toFill, self->_get_s_Xy(), colorToc32(self->get_color()), self->_get_s_Uv(), curvedUIRadius);
                }
            }
        }
    }
    else {
        float x = self->get_transform()->get_localScale().x;
        HMUI::ImageView::AddQuad(toFill, v3tov2(self->_get_s_Xy()->get(0)), v3tov2(self->_get_s_Xy()->get(2)), colorToc32(self->get_color()), v3tov2(self->_get_s_Uv()->get(0)), v3tov2(self->_get_s_Uv()->get(2)), x, curvedUIRadius);
    }
}

#include "HMUI/NoTransitionsButton.hpp"
#include "UnityEngine/UI/Selectable_SelectionState.hpp"

MAKE_HOOK_MATCH(ButtonTransition, &HMUI::NoTransitionsButton::DoStateTransition, void, HMUI::NoTransitionsButton* self, UnityEngine::UI::Selectable::SelectionState state, bool instant) {
    ButtonTransition(self, state, instant);

    if(to_utf8(csstrtostr(self->get_gameObject()->get_name())) != "BSDUIBackButton")
        return;
    
    // set colors for our back button
    auto bg = self->GetComponentsInChildren<UnityEngine::UI::Image*>()->get(0);

    switch (state) {
        case 0: // normal
            bg->set_color({1, 1, 1, 0.2});
            break;
        case 1: // highlighted
            bg->set_color({1, 1, 1, 0.3});
            break;
        case 2: // pressed
            bg->set_color({1, 1, 1, 0.3});
            break;
        case 3: // selected
            bg->set_color({1, 1, 1, 0.3});
            break;
        case 4: // disabled
            bg->set_color({1, 1, 1, 0.1});
            break;
        default:
            bg->set_color({1, 1, 1, 0.2});
            break;
    }
}

extern "C" void setup(ModInfo& info) {
    info.id = ID;
    info.version = VERSION;
    modInfo = info;
	
    getLogger().info("Completed setup!");
}

extern "C" void load() {
    getLogger().info("Starting setup...");
    il2cpp_functions::Init();

    custom_types::Register::AutoRegister();

    // config stuff idk
    getModConfig().Init(modInfo);
    QuestUI::Init();
    QuestUI::Register::RegisterModSettingsViewController(modInfo, DidActivate);

    getLogger().info("Installing hooks...");
    LoggerContextObject logger = getLogger().WithContext("load");
    // Install hooks
    INSTALL_HOOK(logger, ProcessResultsSolo);
    INSTALL_HOOK(logger, ProcessResultsParty);
    INSTALL_HOOK(logger, PostNameResultsParty);
    INSTALL_HOOK(logger, SongStart);
    INSTALL_HOOK(logger, LevelPlay);
    INSTALL_HOOK(logger, LevelPause);
    INSTALL_HOOK(logger, NoteCut);
    INSTALL_HOOK(logger, NoteMiss);
    INSTALL_HOOK(logger, AddScore);
    INSTALL_HOOK(logger, AngleData);
    INSTALL_HOOK(logger, PreSwingCalc);
    INSTALL_HOOK(logger, MakeCompletionResults);
    INSTALL_HOOK(logger, FillImage);
    INSTALL_HOOK(logger, LevelLeaderboardSolo);
    INSTALL_HOOK(logger, LevelLeaderboardParty);
    INSTALL_HOOK(logger, ButtonTransition);
    getLogger().info("Installed all hooks!");
}