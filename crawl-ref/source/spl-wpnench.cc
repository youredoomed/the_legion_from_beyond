/**
 * @file
 * @brief Weapon enchantment spells.
**/

#include "AppHdr.h"

#include "spl-wpnench.h"

#include "areas.h"
#include "artefact.h"
#include "god-item.h"
#include "god-passive.h"
#include "item-prop.h"
#include "makeitem.h"
#include "message.h"
#include "player-equip.h"
#include "prompt.h"
#include "religion.h"
#include "shout.h"
#include "spl-miscast.h"

/** End your weapon branding spell.
 *
 * Returns the weapon to the previous brand, and ends DUR_EXCRUCIATING_WOUNDS.
 * @param weapon The item in question (which may have just been unwielded).
 * @param verbose whether to print a message about expiration.
 */
void end_weapon_brand(item_def &weapon, bool verbose)
{
    ASSERT(you.duration[DUR_EXCRUCIATING_WOUNDS]
    || you.duration[DUR_POISON_WEAPON]);
    bool pain = (you.duration[DUR_EXCRUCIATING_WOUNDS] > 0);

    set_item_ego_type(weapon, OBJ_WEAPONS, you.props[ORIGINAL_BRAND_KEY]);
    you.props.erase(ORIGINAL_BRAND_KEY);
    you.duration[DUR_EXCRUCIATING_WOUNDS] = 0;
    you.duration[DUR_POISON_WEAPON] = 0;

    if (verbose)
    {
        mprf(MSGCH_DURATION, "%s seems less %s.",
             weapon.name(DESC_YOUR).c_str(),
            pain?"pained":"toxic");
    }

    you.wield_change = true;
    const brand_type real_brand = get_weapon_brand(weapon);
    if (real_brand == SPWPN_ANTIMAGIC)
        calc_mp();
    if (you.weapon() && is_holy_item(weapon) && you.form == transformation::lich)
    {
        mprf(MSGCH_DURATION, "%s falls away!", weapon.name(DESC_YOUR).c_str());
        unequip_item(EQ_WEAPON);
    }
}


spret poison_brand_weapon(int power, bool fail)
{
    if (you.duration[DUR_ELEMENTAL_WEAPON]
        || you.duration[DUR_EXCRUCIATING_WOUNDS]) {
        mpr("You are already using a magical weapon.");
        return spret::abort;
    }
    if (you.weapon() == nullptr) {
        mpr("You do not have weapons.");
        return spret::abort;
    }

    item_def& weapon = *you.weapon();

    if (weapon.base_type != OBJ_WEAPONS) {
        mpr("This is not a weapon.");
        return spret::abort;
    }

    const brand_type which_brand = SPWPN_VENOM;
    const brand_type orig_brand = get_weapon_brand(weapon);

    bool has_temp_brand = you.duration[DUR_POISON_WEAPON];
    if (!has_temp_brand && get_weapon_brand(weapon) == which_brand)
    {
        mpr("This weapon is already branded with venom.");
        return spret::abort;
    }

    // But not blowguns.
    if (weapon.sub_type == WPN_BLOWGUN) {
        mpr("You cannot brand blowgun.");
        return spret::abort;
    }

    if (is_artefact(weapon)) {
        mpr("You can't brand this weapon.");
        return spret::abort;
    }

    const bool dangerous_disto = orig_brand == SPWPN_DISTORTION
        && !have_passive(passive_t::safe_distortion);
    if (dangerous_disto)
    {
        const string prompt =
            "Really brand " + weapon.name(DESC_INVENTORY) + "?";
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
    }

    fail_check();

    if (dangerous_disto)
    {
        // Can't get out of it that easily...
        MiscastEffect(&you, nullptr, { miscast_source::wield },
            spschool::translocation, 9, 90,
            "rebranding a weapon of distortion");
    }

    mprf("%s starts dripping with poison.", weapon.name(DESC_YOUR).c_str());

    if (!has_temp_brand)
    {
        you.props[ORIGINAL_BRAND_KEY] = get_weapon_brand(weapon);
        set_item_ego_type(weapon, OBJ_WEAPONS, which_brand);
        you.wield_change = true;
        if (you.duration[DUR_SPWPN_PROTECTION])
        {
            you.duration[DUR_SPWPN_PROTECTION] = 0;
            you.redraw_armour_class = true;
        }
        if (orig_brand == SPWPN_ANTIMAGIC)
            calc_mp();
    }

    you.increase_duration(DUR_POISON_WEAPON, 8 + roll_dice(2, power), 100);

    return spret::success;
}



/**
 * Temporarily brand a weapon with pain.
 *
 * @param[in] power         Spellpower.
 * @param[in] fail          Whether you've already failed to cast.
 * @return                  Success, fail, or abort.
 */
spret cast_excruciating_wounds(int power, bool fail)
{
    if (you.duration[DUR_ELEMENTAL_WEAPON]
        || you.duration[DUR_POISON_WEAPON]) {
        mpr("You are already using a magical weapon.");
        return spret::abort;
    }
    item_def& weapon = *you.weapon();
    const brand_type which_brand = SPWPN_PAIN;
    const brand_type orig_brand = get_weapon_brand(weapon);

    // Can only brand melee weapons.
    if (is_range_weapon(weapon))
    {
        mpr("You cannot brand ranged weapons with this spell.");
        return spret::abort;
    }

    bool has_temp_brand = you.duration[DUR_EXCRUCIATING_WOUNDS];
    if (!has_temp_brand && get_weapon_brand(weapon) == which_brand)
    {
        mpr("This weapon is already branded with pain.");
        return spret::abort;
    }

    const bool dangerous_disto = orig_brand == SPWPN_DISTORTION
                                 && !have_passive(passive_t::safe_distortion);
    if (dangerous_disto)
    {
        const string prompt =
              "Really brand " + weapon.name(DESC_INVENTORY) + "?";
        if (!yesno(prompt.c_str(), false, 'n'))
        {
            canned_msg(MSG_OK);
            return spret::abort;
        }
    }

    fail_check();

    if (dangerous_disto)
    {
        // Can't get out of it that easily...
        MiscastEffect(&you, nullptr, {miscast_source::wield},
                      spschool::translocation, 9, 90,
                      "rebranding a weapon of distortion");
    }

    noisy(spell_effect_noise(SPELL_EXCRUCIATING_WOUNDS), you.pos());
    mprf("%s %s in agony.", weapon.name(DESC_YOUR).c_str(),
                            silenced(you.pos()) ? "writhes" : "shrieks");

    if (!has_temp_brand)
    {
        you.props[ORIGINAL_BRAND_KEY] = get_weapon_brand(weapon);
        set_item_ego_type(weapon, OBJ_WEAPONS, which_brand);
        you.wield_change = true;
        if (you.duration[DUR_SPWPN_PROTECTION])
        {
            you.duration[DUR_SPWPN_PROTECTION] = 0;
            you.redraw_armour_class = true;
        }
        if (orig_brand == SPWPN_ANTIMAGIC)
            calc_mp();
    }

    you.increase_duration(DUR_EXCRUCIATING_WOUNDS, 8 + roll_dice(2, power), 50);

    return spret::success;
}

spret cast_confusing_touch(int power, bool fail)
{
    fail_check();
    msg::stream << you.hands_act("begin", "to glow ")
                << (you.duration[DUR_CONFUSING_TOUCH] ? "brighter" : "red")
                << "." << endl;

    you.set_duration(DUR_CONFUSING_TOUCH,
                     max(10 + random2(power) / 5,
                         you.duration[DUR_CONFUSING_TOUCH]),
                     20, nullptr);

    return spret::success;
}

spret cast_poison_gland(int power, bool fail)
{
    fail_check();
    if (!you.duration[DUR_POISON_GLAND])
        mpr("Your weapon begins to release the poison glands.");
    else
        mpr("You extend your poison gland duration.");

    you.increase_duration(DUR_POISON_GLAND, 10 + roll_dice(2, power/2), 100);
    return spret::success;
}