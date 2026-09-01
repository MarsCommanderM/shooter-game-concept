#include <AzTest/AzTest.h>
#include <STWGameplay/ViewmodelPresentation.h>
#include <STWGameplay/PlayerSliceModel.h>
#include <AzCore/std/algorithm.h>
#include <cmath>

// AZ_UNIT_TEST_HOOK is defined once for this test module in PlayerSliceModelTests.cpp.

namespace STWGameplay
{
    namespace
    {
        PresentationInput ShotInput()
        {
            PresentationInput in;
            in.m_shotFired = true;
            return in;
        }

        void Advance(ViewmodelPresentation& vm, float seconds, PresentationInput input = {})
        {
            constexpr float step = 1.0f / 120.0f;
            for (float elapsed = 0.0f; elapsed < seconds; elapsed += step)
            {
                ASSERT_TRUE(vm.Update(AZStd::min(step, seconds - elapsed), input));
            }
        }
    }

    // A. A rejected/absent shot produces no fire presentation event.
    TEST(ViewmodelPresentationTests, RejectedShotProducesNoFireEvent)
    {
        ViewmodelPresentation vm;
        PresentationInput idle;
        ASSERT_TRUE(vm.Update(0.016f, idle));
        EXPECT_EQ(vm.GetFireEventCount(), 0u);
        EXPECT_FALSE(vm.IsMuzzleFlashActive());
        EXPECT_NE(vm.GetState(), ViewmodelState::Fire);
        EXPECT_TRUE(vm.GetRecoilOffset().IsZero());
        EXPECT_FLOAT_EQ(vm.GetRecoilPitch(), 0.0f);
    }

    TEST(ViewmodelPresentationTests, IdleHipPoseIsStable)
    {
        ViewmodelPresentation vm;
        const AZ::Vector3 expected(
            ViewmodelPresentation::HipPoseRight,
            ViewmodelPresentation::HipPoseForward,
            ViewmodelPresentation::HipPoseUp);
        EXPECT_TRUE(vm.GetPoseOffset().IsClose(expected));
        Advance(vm, 3.0f);
        EXPECT_TRUE(vm.GetPoseOffset().IsClose(expected));
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 0.0f);
    }

    TEST(ViewmodelPresentationTests, SingleFireAppliesImmediateFullImpulse)
    {
        ViewmodelPresentation vm;
        ASSERT_TRUE(vm.Update(1.0f / 30.0f, ShotInput()));
        EXPECT_TRUE(vm.GetRecoilOffset().IsClose(
            AZ::Vector3(0.0f, -ViewmodelPresentation::RecoilKick,
                ViewmodelPresentation::RecoilKick * 0.5f)));
        EXPECT_FLOAT_EQ(vm.GetRecoilPitch(), ViewmodelPresentation::RecoilPitchKick);
    }

    TEST(ViewmodelPresentationTests, RecoilRecoveryIsFrameRateIndependent)
    {
        ViewmodelPresentation coarse;
        ViewmodelPresentation fine;
        ASSERT_TRUE(coarse.Update(0.0f, ShotInput()));
        ASSERT_TRUE(fine.Update(0.0f, ShotInput()));
        ASSERT_TRUE(coarse.Update(0.25f, {}));
        Advance(fine, 0.25f);
        EXPECT_TRUE(coarse.GetRecoilOffset().IsClose(fine.GetRecoilOffset(), 0.00001f));
        EXPECT_NEAR(coarse.GetRecoilPitch(), fine.GetRecoilPitch(), 0.00001f);
    }

    TEST(ViewmodelPresentationTests, RepeatedFireAccumulatesRecoil)
    {
        ViewmodelPresentation vm;
        ASSERT_TRUE(vm.Update(0.0f, ShotInput()));
        const float first = vm.GetRecoilOffset().GetLength();
        ASSERT_TRUE(vm.Update(0.0f, ShotInput()));
        EXPECT_GT(vm.GetRecoilOffset().GetLength(), first);
        EXPECT_FLOAT_EQ(vm.GetRecoilPitch(), ViewmodelPresentation::RecoilPitchKick * 2.0f);
    }

    TEST(ViewmodelPresentationTests, MuzzleFlashExpiresAtDeterministicBoundary)
    {
        ViewmodelPresentation vm;
        ASSERT_TRUE(vm.Update(0.0f, ShotInput()));
        EXPECT_TRUE(vm.IsMuzzleFlashActive());
        EXPECT_FLOAT_EQ(vm.GetMuzzleFlashRemaining(), ViewmodelPresentation::MuzzleFlashDuration);
        ASSERT_TRUE(vm.Update(ViewmodelPresentation::MuzzleFlashDuration, {}));
        EXPECT_FALSE(vm.IsMuzzleFlashActive());
        EXPECT_FLOAT_EQ(vm.GetMuzzleFlashRemaining(), 0.0f);
    }

    TEST(ViewmodelPresentationTests, ReloadDoesNotTriggerFirePresentation)
    {
        ViewmodelPresentation vm;
        PresentationInput reload;
        reload.m_reloading = true;
        ASSERT_TRUE(vm.Update(0.016f, reload));
        EXPECT_EQ(vm.GetReloadStartCount(), 1u);
        EXPECT_EQ(vm.GetFireEventCount(), 0u);
        EXPECT_TRUE(vm.GetRecoilOffset().IsZero());
        EXPECT_FALSE(vm.IsMuzzleFlashActive());
    }

    TEST(ViewmodelPresentationTests, AdsBlendIsReadyButDefaultsToHip)
    {
        ViewmodelPresentation vm;
        Advance(vm, 1.0f);
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 0.0f);
        PresentationInput ads;
        ads.m_adsRequested = true;
        Advance(vm, 1.0f, ads);
        EXPECT_GT(vm.GetAdsBlend(), 0.99f);
        EXPECT_TRUE(vm.GetPoseOffset().IsClose(AZ::Vector3(
            ViewmodelPresentation::AdsPoseRight,
            ViewmodelPresentation::AdsPoseForward,
            ViewmodelPresentation::AdsPoseUp), 0.001f));
    }

    TEST(ViewmodelPresentationTests, AdsPressStartsBlendTowardOne)
    {
        ViewmodelPresentation vm;
        PresentationInput ads;
        ads.m_adsRequested = true;
        ASSERT_TRUE(vm.Update(1.0f / 120.0f, ads));
        EXPECT_GT(vm.GetAdsBlend(), 0.0f);
        EXPECT_LT(vm.GetAdsBlend(), 1.0f);
        EXPECT_LT(vm.GetCameraFovDegrees(), ViewmodelPresentation::HipCameraFovDegrees);
    }

    TEST(ViewmodelPresentationTests, AdsHoldReachesStableEndpoint)
    {
        ViewmodelPresentation vm;
        PresentationInput ads;
        ads.m_adsRequested = true;
        Advance(vm, 1.0f, ads);
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 1.0f);
        EXPECT_FLOAT_EQ(vm.GetCameraFovDegrees(), ViewmodelPresentation::AdsCameraFovDegrees);
        Advance(vm, 2.0f, ads);
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 1.0f);
    }

    TEST(ViewmodelPresentationTests, AdsReleaseReturnsToStableHipEndpoint)
    {
        ViewmodelPresentation vm;
        PresentationInput ads;
        ads.m_adsRequested = true;
        Advance(vm, 1.0f, ads);
        ASSERT_TRUE(vm.Update(1.0f / 120.0f, {}));
        EXPECT_GT(vm.GetAdsBlend(), 0.0f);
        EXPECT_LT(vm.GetAdsBlend(), 1.0f);
        Advance(vm, 1.0f);
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 0.0f);
        EXPECT_FLOAT_EQ(vm.GetCameraFovDegrees(), ViewmodelPresentation::HipCameraFovDegrees);
        Advance(vm, 2.0f);
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 0.0f);
    }

    TEST(ViewmodelPresentationTests, AdsBlendRemainsClamped)
    {
        ViewmodelPresentation vm;
        PresentationInput ads;
        ads.m_adsRequested = true;
        Advance(vm, 20.0f, ads);
        EXPECT_GE(vm.GetAdsBlend(), 0.0f);
        EXPECT_LE(vm.GetAdsBlend(), 1.0f);
        Advance(vm, 20.0f);
        EXPECT_GE(vm.GetAdsBlend(), 0.0f);
        EXPECT_LE(vm.GetAdsBlend(), 1.0f);
    }

    TEST(ViewmodelPresentationTests, AdsBlendIsFrameRateIndependent)
    {
        ViewmodelPresentation coarse;
        ViewmodelPresentation fine;
        PresentationInput ads;
        ads.m_adsRequested = true;
        ASSERT_TRUE(coarse.Update(0.25f, ads));
        Advance(fine, 0.25f, ads);
        EXPECT_NEAR(coarse.GetAdsBlend(), fine.GetAdsBlend(), 0.00001f);
        EXPECT_TRUE(coarse.GetPoseOffset().IsClose(fine.GetPoseOffset(), 0.00001f));
        EXPECT_NEAR(coarse.GetCameraFovDegrees(), fine.GetCameraFovDegrees(), 0.00001f);
    }

    TEST(ViewmodelPresentationTests, FireDuringAdsPreservesBlendAndAppliesRecoil)
    {
        ViewmodelPresentation vm;
        PresentationInput ads;
        ads.m_adsRequested = true;
        Advance(vm, 1.0f, ads);
        PresentationInput adsShot = ads;
        adsShot.m_shotFired = true;
        ASSERT_TRUE(vm.Update(0.0f, adsShot));
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 1.0f);
        EXPECT_GT(vm.GetRecoilOffset().GetLength(), 0.0f);
        EXPECT_TRUE(vm.IsMuzzleFlashActive());
    }

    TEST(ViewmodelPresentationTests, ReleasingAdsDuringRecoilRecoversBothStates)
    {
        ViewmodelPresentation vm;
        PresentationInput ads;
        ads.m_adsRequested = true;
        Advance(vm, 1.0f, ads);
        PresentationInput adsShot = ads;
        adsShot.m_shotFired = true;
        ASSERT_TRUE(vm.Update(0.0f, adsShot));
        const float recoil = vm.GetRecoilOffset().GetLength();
        Advance(vm, 0.25f);
        EXPECT_LT(vm.GetAdsBlend(), 1.0f);
        EXPECT_LT(vm.GetRecoilOffset().GetLength(), recoil);
        Advance(vm, 1.0f);
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 0.0f);
        EXPECT_LT(vm.GetRecoilOffset().GetLength(), 0.001f);
    }

    TEST(ViewmodelPresentationTests, EnteringAdsDuringRecoilRecoveryPreservesRecoil)
    {
        ViewmodelPresentation vm;
        ASSERT_TRUE(vm.Update(0.0f, ShotInput()));
        const float kicked = vm.GetRecoilOffset().GetLength();
        PresentationInput ads;
        ads.m_adsRequested = true;
        ASSERT_TRUE(vm.Update(0.05f, ads));
        EXPECT_GT(vm.GetAdsBlend(), 0.0f);
        EXPECT_GT(vm.GetRecoilOffset().GetLength(), 0.0f);
        EXPECT_LT(vm.GetRecoilOffset().GetLength(), kicked);
    }

    TEST(ViewmodelPresentationTests, ReloadDoesNotCorruptAdsBlend)
    {
        ViewmodelPresentation vm;
        PresentationInput adsReload;
        adsReload.m_adsRequested = true;
        adsReload.m_reloading = true;
        Advance(vm, 1.0f, adsReload);
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 1.0f);
        EXPECT_EQ(vm.GetState(), ViewmodelState::Reload);
        adsReload.m_adsRequested = false;
        Advance(vm, 1.0f, adsReload);
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 0.0f);
        EXPECT_EQ(vm.GetState(), ViewmodelState::Reload);
    }

    TEST(ViewmodelPresentationTests, AdsPresentationDoesNotModifyGameplayAuthority)
    {
        PlayerSliceModel model;
        ViewmodelPresentation vm;
        const WeaponState weapon = model.GetWeapon();
        const TargetState target = model.GetTarget();
        PresentationInput ads;
        ads.m_adsRequested = true;
        Advance(vm, PlayerSliceModel::FireInterval * 2.0f, ads);
        EXPECT_EQ(model.GetWeapon().m_magazine, weapon.m_magazine);
        EXPECT_EQ(model.GetWeapon().m_reserve, weapon.m_reserve);
        EXPECT_FLOAT_EQ(model.GetWeapon().m_cooldownRemaining, weapon.m_cooldownRemaining);
        EXPECT_FLOAT_EQ(model.GetTarget().m_health, target.m_health);
    }

    TEST(ViewmodelPresentationTests, VerificationStimulusIsDisabledByDefault)
    {
        ViewmodelPresentation vm;
        PresentationInput idle;
        for (int i = 0; i < 120; ++i)
        {
            ASSERT_TRUE(vm.Update(1.0f / 120.0f, idle));
        }
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 0.0f);
        EXPECT_EQ(vm.GetFireEventCount(), 0u);
    }

    TEST(ViewmodelPresentationTests, VerificationSequenceReachesAdsAndReturnsToHip)
    {
        ViewmodelPresentation vm;
        PresentationInput input;
        for (int i = 0; i < 120; ++i) { ASSERT_TRUE(vm.Update(1.0f / 120.0f, input)); }
        input.m_adsRequested = true;
        for (int i = 0; i < 600; ++i) { ASSERT_TRUE(vm.Update(1.0f / 120.0f, input)); }
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 1.0f);
        EXPECT_FLOAT_EQ(vm.GetCameraFovDegrees(), ViewmodelPresentation::AdsCameraFovDegrees);
        EXPECT_TRUE(vm.GetPoseOffset().IsClose(AZ::Vector3(
            ViewmodelPresentation::AdsPoseRight, ViewmodelPresentation::AdsPoseForward,
            ViewmodelPresentation::AdsPoseUp), 0.001f));
        input.m_shotFired = true;
        ASSERT_TRUE(vm.Update(0.0f, input));
        EXPECT_EQ(vm.GetFireEventCount(), 1u);
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 1.0f);
        input.m_shotFired = false;
        input.m_adsRequested = false;
        for (int i = 0; i < 600; ++i) { ASSERT_TRUE(vm.Update(1.0f / 120.0f, input)); }
        EXPECT_FLOAT_EQ(vm.GetAdsBlend(), 0.0f);
        EXPECT_FLOAT_EQ(vm.GetCameraFovDegrees(), ViewmodelPresentation::HipCameraFovDegrees);
        EXPECT_TRUE(vm.GetPoseOffset().IsClose(AZ::Vector3(
            ViewmodelPresentation::HipPoseRight, ViewmodelPresentation::HipPoseForward,
            ViewmodelPresentation::HipPoseUp), 0.001f));
    }

    TEST(ViewmodelPresentationTests, VerificationSequenceIsDeterministic)
    {
        ViewmodelPresentation first;
        ViewmodelPresentation second;
        PresentationInput ads; ads.m_adsRequested = true;
        for (int i = 0; i < 90; ++i)
        {
            ASSERT_TRUE(first.Update(1.0f / 120.0f, ads));
            ASSERT_TRUE(second.Update(1.0f / 120.0f, ads));
        }
        EXPECT_FLOAT_EQ(first.GetAdsBlend(), second.GetAdsBlend());
        EXPECT_TRUE(first.GetPoseOffset().IsClose(second.GetPoseOffset(), 0.000001f));
        EXPECT_FLOAT_EQ(first.GetCameraFovDegrees(), second.GetCameraFovDegrees());
        EXPECT_EQ(first.GetFireEventCount(), second.GetFireEventCount());
    }

    // B. A valid shot produces exactly one fire presentation event.
    TEST(ViewmodelPresentationTests, ValidShotProducesExactlyOneFireEvent)
    {
        ViewmodelPresentation vm;
        ASSERT_TRUE(vm.Update(0.016f, ShotInput()));
        EXPECT_EQ(vm.GetFireEventCount(), 1u);
        EXPECT_TRUE(vm.IsMuzzleFlashActive());
        PresentationInput idle;
        ASSERT_TRUE(vm.Update(0.016f, idle));
        EXPECT_EQ(vm.GetFireEventCount(), 1u);
    }

    // C. Reload presentation begins only when authoritative reload begins (rising edge).
    TEST(ViewmodelPresentationTests, ReloadStartTracksAuthoritativeRisingEdge)
    {
        ViewmodelPresentation vm;
        PresentationInput reloading;
        reloading.m_reloading = true;
        ASSERT_TRUE(vm.Update(0.016f, reloading));
        EXPECT_EQ(vm.GetReloadStartCount(), 1u);
        EXPECT_EQ(vm.GetState(), ViewmodelState::Reload);
        ASSERT_TRUE(vm.Update(0.016f, reloading));
        EXPECT_EQ(vm.GetReloadStartCount(), 1u); // held, not re-triggered
    }

    // D. Reload presentation cannot duplicate ammo: authoritative reload conserves ammo while
    //    the presentation is driven from its state.
    TEST(ViewmodelPresentationTests, PresentationCannotDuplicateAmmoDuringReload)
    {
        PlayerSliceModel model;
        ViewmodelPresentation vm;
        ASSERT_TRUE(model.TryFire());
        ASSERT_TRUE(model.StartReload());
        const int before = model.GetWeapon().m_magazine + model.GetWeapon().m_reserve;
        constexpr float step = 1.0f / 120.0f;
        for (float t = 0.0f; t < PlayerSliceModel::ReloadDuration + 0.05f; t += step)
        {
            ASSERT_TRUE(model.Update(step, {}));
            PresentationInput in;
            in.m_reloading = model.GetWeapon().m_reloading;
            ASSERT_TRUE(vm.Update(step, in));
        }
        EXPECT_EQ(model.GetWeapon().m_magazine, 30);
        EXPECT_EQ(model.GetWeapon().m_magazine + model.GetWeapon().m_reserve, before);
    }

    // E. Presentation cannot modify target health.
    TEST(ViewmodelPresentationTests, PresentationCannotModifyTargetHealth)
    {
        PlayerSliceModel model;
        ViewmodelPresentation vm;
        ASSERT_TRUE(model.TryFire());
        const float health = model.GetTarget().m_health;
        Advance(vm, 0.5f, ShotInput());
        // The presentation consumed the fire event but never touched authoritative target state.
        EXPECT_FLOAT_EQ(model.GetTarget().m_health, health);
        EXPECT_GT(vm.GetFireEventCount(), 0u);
    }

    // F. Presentation cannot modify magazine/reserve directly.
    TEST(ViewmodelPresentationTests, PresentationCannotModifyAmmo)
    {
        PlayerSliceModel model;
        ViewmodelPresentation vm;
        ASSERT_TRUE(model.TryFire());
        const int magazine = model.GetWeapon().m_magazine;
        const int reserve = model.GetWeapon().m_reserve;
        Advance(vm, 0.5f, ShotInput());
        EXPECT_EQ(model.GetWeapon().m_magazine, magazine);
        EXPECT_EQ(model.GetWeapon().m_reserve, reserve);
    }

    // G. Recoil stays bounded under sustained fire.
    TEST(ViewmodelPresentationTests, RecoilStaysBounded)
    {
        ViewmodelPresentation vm;
        for (int shot = 0; shot < 200; ++shot)
        {
            ASSERT_TRUE(vm.Update(1.0f / 120.0f, ShotInput()));
            EXPECT_LE(vm.GetRecoilOffset().GetLength(), ViewmodelPresentation::MaxRecoil + 0.0001f);
            EXPECT_LE(std::abs(vm.GetRecoilPitch()), ViewmodelPresentation::MaxRecoil + 0.0001f);
        }
    }

    // H. Recoil returns toward neutral after firing stops.
    TEST(ViewmodelPresentationTests, RecoilReturnsTowardNeutral)
    {
        ViewmodelPresentation vm;
        ASSERT_TRUE(vm.Update(0.016f, ShotInput()));
        const float kicked = vm.GetRecoilOffset().GetLength();
        EXPECT_GT(kicked, 0.0f);
        Advance(vm, 1.0f); // idle
        EXPECT_LT(vm.GetRecoilOffset().GetLength(), kicked);
        EXPECT_LT(vm.GetRecoilOffset().GetLength(), 0.01f);
    }

    // I. Weapon motion remains finite under prolonged mixed input.
    TEST(ViewmodelPresentationTests, WeaponMotionRemainsFinite)
    {
        ViewmodelPresentation vm;
        PresentationInput moving; moving.m_moving = true; moving.m_sprinting = true;
        for (int i = 0; i < 2000; ++i)
        {
            PresentationInput in = moving;
            in.m_shotFired = (i % 7 == 0);
            ASSERT_TRUE(vm.Update(1.0f / 120.0f, in));
        }
        EXPECT_TRUE(vm.GetRecoilOffset().IsFinite());
        EXPECT_TRUE(vm.GetSwayOffset().IsFinite());
        EXPECT_TRUE(std::isfinite(vm.GetRecoilPitch()));
        EXPECT_LE(vm.GetSwayOffset().GetLength(), 0.2f);
    }

    // J. Sprint state selects sprint presentation.
    TEST(ViewmodelPresentationTests, SprintSelectsSprintState)
    {
        ViewmodelPresentation vm;
        PresentationInput sprint; sprint.m_moving = true; sprint.m_sprinting = true;
        ASSERT_TRUE(vm.Update(0.016f, sprint));
        EXPECT_EQ(vm.GetState(), ViewmodelState::Sprint);
        PresentationInput walk; walk.m_moving = true;
        ASSERT_TRUE(vm.Update(0.016f, walk));
        EXPECT_EQ(vm.GetState(), ViewmodelState::Move);
    }

    // K. A fire event temporarily overrides locomotion presentation.
    TEST(ViewmodelPresentationTests, FireOverridesLocomotionBriefly)
    {
        ViewmodelPresentation vm;
        PresentationInput movingShot; movingShot.m_moving = true; movingShot.m_sprinting = true;
        movingShot.m_shotFired = true;
        ASSERT_TRUE(vm.Update(0.016f, movingShot));
        EXPECT_EQ(vm.GetState(), ViewmodelState::Fire);
        // After the cue elapses while still moving, it returns to the locomotion state.
        PresentationInput moving; moving.m_moving = true; moving.m_sprinting = true;
        Advance(vm, 0.3f, moving);
        EXPECT_EQ(vm.GetState(), ViewmodelState::Sprint);
    }

    // L. Reload state transitions cleanly back to locomotion/idle when authoritative reload ends.
    TEST(ViewmodelPresentationTests, ReloadTransitionsBackToLocomotion)
    {
        ViewmodelPresentation vm;
        PresentationInput reloading; reloading.m_reloading = true;
        Advance(vm, 0.2f, reloading);
        EXPECT_EQ(vm.GetState(), ViewmodelState::Reload);
        PresentationInput idle;
        ASSERT_TRUE(vm.Update(0.016f, idle));
        EXPECT_EQ(vm.GetState(), ViewmodelState::Idle);
    }
}
