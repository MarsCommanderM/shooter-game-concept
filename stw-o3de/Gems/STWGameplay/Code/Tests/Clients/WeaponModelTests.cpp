#include <AzTest/AzTest.h>
#include <STWGameplay/WeaponModel.h>

namespace STWGameplay
{
    TEST(WeaponModelTests, DefaultLoadoutMatchesExistingSTWContract)
    {
        WeaponModel model;
        EXPECT_EQ(model.GetLoadoutProfile(EquipmentSlot::Primary), EquipmentProfileId::STW_SMG_01);
        EXPECT_EQ(model.GetLoadoutProfile(EquipmentSlot::Secondary), EquipmentProfileId::STW_RIFLE_02);
        EXPECT_EQ(model.GetLoadoutProfile(EquipmentSlot::Tactical), EquipmentProfileId::STW_TACTICAL_FLASH_01);
        EXPECT_EQ(model.GetLoadoutProfile(EquipmentSlot::Lethal), EquipmentProfileId::STW_LETHAL_FRAG_01);
        EXPECT_EQ(model.GetLoadoutProfile(EquipmentSlot::Melee), EquipmentProfileId::STW_MELEE_01);
        EXPECT_EQ(model.GetWeapon().m_magazine, 30);
        EXPECT_EQ(model.GetWeapon().m_reserve, 150);
    }

    TEST(WeaponModelTests, AcceptedMagazineUseConsumesExactlyOneRound)
    {
        WeaponModel model;
        WeaponUseResult result;
        ASSERT_TRUE(model.TryUse(true, result));
        EXPECT_TRUE(result.m_accepted);
        EXPECT_TRUE(result.m_shotFired);
        EXPECT_TRUE(result.m_equipmentUsed);
        EXPECT_EQ(model.GetWeapon().m_magazine, 29);
        EXPECT_NE(result.m_eventId, 0u);
    }

    TEST(WeaponModelTests, CooldownRejectsRepeatedUse)
    {
        WeaponModel model;
        WeaponUseResult first;
        WeaponUseResult rejected;
        ASSERT_TRUE(model.TryUse(true, first));
        EXPECT_FALSE(model.TryUse(true, rejected));
        EXPECT_EQ(model.GetWeapon().m_magazine, 29);
        EXPECT_EQ(rejected.m_eventId, 0u);
    }

    TEST(WeaponModelTests, ReloadConservesTotalAmmo)
    {
        WeaponModel model;
        WeaponUseResult use;
        ASSERT_TRUE(model.TryUse(true, use));
        ASSERT_TRUE(model.Update(0.075f));
        ASSERT_TRUE(model.StartReload(true));
        const int totalBefore = model.GetWeapon().m_magazine + model.GetWeapon().m_reserve;
        ASSERT_TRUE(model.Update(1.76f));
        EXPECT_EQ(model.GetWeapon().m_magazine, 30);
        EXPECT_EQ(model.GetWeapon().m_magazine + model.GetWeapon().m_reserve, totalBefore);
    }

    TEST(WeaponModelTests, TacticalUseConsumesChargeWithoutShotSignal)
    {
        WeaponModel model;
        ASSERT_TRUE(model.RequestEquipmentSwitch(EquipmentSlot::Tactical, true));
        WeaponUseResult result;
        ASSERT_TRUE(model.TryUse(true, result));
        EXPECT_TRUE(result.m_accepted);
        EXPECT_TRUE(result.m_equipmentUsed);
        EXPECT_FALSE(result.m_shotFired);
        EXPECT_EQ(model.GetWeapon().m_charges, 1);
    }

    TEST(WeaponModelTests, MeleeUseRequiresNoAmmoResource)
    {
        WeaponModel model;
        ASSERT_TRUE(model.RequestEquipmentSwitch(EquipmentSlot::Melee, true));
        WeaponUseResult result;
        ASSERT_TRUE(model.TryUse(true, result));
        EXPECT_TRUE(result.m_accepted);
        EXPECT_TRUE(result.m_equipmentUsed);
        EXPECT_FALSE(result.m_shotFired);
        EXPECT_FLOAT_EQ(result.m_range, 2.5f);
        EXPECT_FLOAT_EQ(result.m_damage, 50.0f);
    }

    TEST(WeaponModelTests, ReloadBlocksEquipmentSwitch)
    {
        WeaponModel model;
        WeaponUseResult use;
        ASSERT_TRUE(model.TryUse(true, use));
        ASSERT_TRUE(model.Update(0.075f));
        ASSERT_TRUE(model.StartReload(true));
        EXPECT_FALSE(model.RequestEquipmentSwitch(EquipmentSlot::Secondary, true));
        EXPECT_EQ(model.GetActiveEquipmentSlot(), EquipmentSlot::Primary);
    }

    TEST(WeaponModelTests, SwitchingCannotBypassExistingCooldownPolicy)
    {
        WeaponModel model;
        WeaponUseResult first;
        ASSERT_TRUE(model.TryUse(true, first));
        ASSERT_TRUE(model.RequestEquipmentSwitch(EquipmentSlot::Secondary, true));
        ASSERT_TRUE(model.RequestEquipmentSwitch(EquipmentSlot::Primary, true));
        WeaponUseResult second;
        EXPECT_FALSE(model.TryUse(true, second));
        EXPECT_GT(model.GetWeapon().m_cooldownRemaining, 0.0f);
    }

    TEST(WeaponModelTests, InvalidSlotProfileCombinationIsRejected)
    {
        WeaponModel model;
        EXPECT_FALSE(model.SetLoadoutProfile(EquipmentSlot::Tactical, EquipmentProfileId::STW_RIFLE_03));
        EXPECT_FALSE(model.RequestEquipmentSwitch(static_cast<EquipmentSlot>(255), true));
    }

    TEST(WeaponModelTests, AcceptedUseIdsAreMonotonic)
    {
        WeaponModel model;
        WeaponUseResult first;
        WeaponUseResult second;
        ASSERT_TRUE(model.TryUse(true, first));
        ASSERT_TRUE(model.Update(model.GetActiveEquipmentProfile().m_fireInterval));
        ASSERT_TRUE(model.TryUse(true, second));
        EXPECT_GT(second.m_eventId, first.m_eventId);
        EXPECT_EQ(model.GetLastAcceptedUseEventId(), second.m_eventId);
    }

    TEST(WeaponModelTests, InvalidDeltaLeavesWeaponStateUntouched)
    {
        WeaponModel model;
        WeaponUseResult use;
        ASSERT_TRUE(model.TryUse(true, use));
        const WeaponState before = model.GetWeapon();
        EXPECT_FALSE(model.Update(-0.1f));
        EXPECT_EQ(model.GetWeapon().m_magazine, before.m_magazine);
        EXPECT_FLOAT_EQ(model.GetWeapon().m_cooldownRemaining, before.m_cooldownRemaining);
        EXPECT_EQ(model.GetWeapon().m_reloading, before.m_reloading);
    }
}
