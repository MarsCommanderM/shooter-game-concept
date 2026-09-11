#include <AzTest/AzTest.h>
#include <AzCore/std/algorithm.h>
#include <STWGameplay/PlayerMovementModel.h>

namespace STWGameplay
{
    namespace
    {
        PlayerMovementInput ForwardInput(bool grounded = true)
        {
            PlayerMovementInput input;
            input.m_direction = AZ::Vector3::CreateAxisY();
            input.m_grounded = grounded;
            return input;
        }

        void Advance(PlayerMovementModel& model, float seconds, const PlayerMovementInput& input)
        {
            constexpr float step = 1.0f / 120.0f;
            for (float elapsed = 0.0f; elapsed < seconds; elapsed += step)
            {
                EXPECT_TRUE(model.Update(AZStd::min(step, seconds - elapsed), input));
            }
        }
    }

    TEST(PlayerMovementModelTests, WalkReachesConfiguredTargetSpeed)
    {
        PlayerMovementModel model;
        Advance(model, 1.0f, ForwardInput());
        EXPECT_NEAR(model.GetVelocity().GetLength(), model.GetConfig().m_walkSpeed, 0.001f);
    }

    TEST(PlayerMovementModelTests, SprintTransitionIsDeterministic)
    {
        PlayerMovementModel first;
        PlayerMovementModel second;
        const PlayerMovementInput walk = ForwardInput();
        PlayerMovementInput sprint = walk;
        sprint.m_sprint = true;
        Advance(first, 0.25f, walk);
        Advance(second, 0.25f, walk);
        Advance(first, 0.5f, sprint);
        Advance(second, 0.5f, sprint);
        EXPECT_TRUE(first.GetVelocity().IsClose(second.GetVelocity(), 0.0001f));
        EXPECT_NEAR(first.GetVelocity().GetLength(), first.GetConfig().m_sprintSpeed, 0.001f);
    }

    TEST(PlayerMovementModelTests, ReleasingInputDeceleratesInsteadOfStoppingInstantly)
    {
        PlayerMovementModel model;
        const PlayerMovementInput moving = ForwardInput();
        Advance(model, 0.5f, moving);
        const float movingSpeed = model.GetVelocity().GetLength();
        const PlayerMovementInput released;
        ASSERT_TRUE(model.Update(0.05f, released));
        EXPECT_GT(model.GetVelocity().GetLength(), 0.0f);
        EXPECT_LT(model.GetVelocity().GetLength(), movingSpeed);
    }

    TEST(PlayerMovementModelTests, ReverseDirectionBrakesResponsively)
    {
        PlayerMovementModel model;
        Advance(model, 0.5f, ForwardInput());
        PlayerMovementInput reverse = ForwardInput();
        reverse.m_direction = -AZ::Vector3::CreateAxisY();
        ASSERT_TRUE(model.Update(0.2f, reverse));
        EXPECT_LT(model.GetVelocity().GetY(), 0.0f);
    }

    TEST(PlayerMovementModelTests, AirborneAccelerationIsLowerThanGroundedAcceleration)
    {
        PlayerMovementModel grounded;
        PlayerMovementModel airborne;
        ASSERT_TRUE(grounded.Update(0.1f, ForwardInput(true)));
        ASSERT_TRUE(airborne.Update(0.1f, ForwardInput(false)));
        EXPECT_LT(airborne.GetVelocity().GetLength(), grounded.GetVelocity().GetLength());
    }

    TEST(PlayerMovementModelTests, AirInputCannotCreateUnboundedSpeed)
    {
        PlayerMovementModel model;
        Advance(model, 10.0f, ForwardInput(false));
        EXPECT_LE(model.GetVelocity().GetLength(), model.GetConfig().m_maxAirSpeed + 0.001f);
    }

    TEST(PlayerMovementModelTests, DiagonalMovementRemainsNormalized)
    {
        PlayerMovementModel model;
        PlayerMovementInput diagonal = ForwardInput();
        diagonal.m_direction = AZ::Vector3(1.0f, 1.0f, 0.0f);
        Advance(model, 1.0f, diagonal);
        EXPECT_NEAR(model.GetVelocity().GetLength(), model.GetConfig().m_walkSpeed, 0.001f);
    }

    TEST(PlayerMovementModelTests, InvalidDeltaLeavesStateUnchanged)
    {
        PlayerMovementModel model;
        Advance(model, 0.25f, ForwardInput());
        const AZ::Vector3 before = model.GetVelocity();
        EXPECT_FALSE(model.Update(-0.1f, ForwardInput()));
        EXPECT_TRUE(model.GetVelocity().IsClose(before, 0.0001f));
    }

    TEST(PlayerMovementModelTests, DeadInputClearsVelocity)
    {
        PlayerMovementModel model;
        Advance(model, 0.25f, ForwardInput());
        PlayerMovementInput dead = ForwardInput();
        dead.m_alive = false;
        ASSERT_TRUE(model.Update(0.016f, dead));
        EXPECT_TRUE(model.GetVelocity().IsZero());
    }
}
