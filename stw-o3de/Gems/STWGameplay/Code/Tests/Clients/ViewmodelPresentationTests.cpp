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
