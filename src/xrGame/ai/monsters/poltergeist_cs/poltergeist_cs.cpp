#include "stdafx.h"

#include "poltergeist_cs.h"
#include "poltergeist_cs_state_manager.h"
#include "../../../characterphysicssupport.h"
#include "../../../PHMovementControl.h"
#include "../../../PhysicsShellHolder.h"
#include "../../../ai_debug.h"
#include "poltergeist_cs_movement.h"
#include "../../../detail_path_manager.h"
#include "../monster_velocity_space.h"
#include "../../../level.h"
#include "../../../level_debug.h"
#include "../control_animation_base.h"
#include "../control_movement_base.h"
#include "../control_path_builder_base.h"
#include "../../../../xrphysics/PhysicsShell.h"
#include "../../../actor.h"
#include "../../../actor_memory.h"
#include "../../../visual_memory_manager.h"
#include "ActorEffector.h"
#include "../../../ActorCondition.h"

#include "../../../../xrCore/_vector3d_ext.h"
#include "../control_direction_base.h"

#define HEIGHT_CHANGE_VELOCITY 0.5f
#define HEIGHT_CHANGE_MIN_TIME 3000
#define HEIGHT_CHANGE_MAX_TIME 10000
#define HEIGHT_MIN 0.4f
#define HEIGHT_MAX 2.0f

CPoltergeist_cs::CPoltergeist_cs()
{
    StateMan = xr_new<CStateManagerPoltergeist_cs>(this);

    invisible_vel.set(0.1f, 0.1f);

    m_flame = 0;
    m_tele_cs = 0;
}

CPoltergeist_cs::~CPoltergeist_cs()
{
    xr_delete(StateMan);
    xr_delete(m_flame);
    xr_delete(m_tele_cs);
}

void CPoltergeist_cs::Load(LPCSTR section)
{
    inherited::Load(section);

    anim().accel_load(section);
    anim().accel_chain_add(eAnimWalkFwd, eAnimRun);

    invisible_vel.set(pSettings->r_float(section, "Velocity_Invisible_Linear"),
        pSettings->r_float(section, "Velocity_Invisible_Angular"));
    movement().detail().add_velocity(MonsterMovement::eVelocityParameterInvisible,
        CDetailPathManager::STravelParams(invisible_vel.linear, invisible_vel.angular));

    anim().AddReplacedAnim(&m_bDamaged, eAnimWalkFwd, eAnimWalkDamaged);
    anim().AddReplacedAnim(&m_bDamaged, eAnimRun, eAnimRunDamaged);

    SVelocityParam& velocity_none = move().get_velocity(MonsterMovement::eVelocityParameterIdle);
    SVelocityParam& velocity_turn = move().get_velocity(MonsterMovement::eVelocityParameterStand);
    SVelocityParam& velocity_walk = move().get_velocity(MonsterMovement::eVelocityParameterWalkNormal);
    SVelocityParam& velocity_run = move().get_velocity(MonsterMovement::eVelocityParameterRunNormal);
    SVelocityParam& velocity_walk_dmg = move().get_velocity(MonsterMovement::eVelocityParameterWalkDamaged);
    SVelocityParam& velocity_run_dmg = move().get_velocity(MonsterMovement::eVelocityParameterRunDamaged);

    anim().AddAnim(eAnimStandIdle, "stand_idle_", -1, &velocity_none, PS_STAND, "fx_stand_f", "fx_stand_b",
        "fx_stand_l", "fx_stand_r");
    anim().AddAnim(eAnimStandTurnLeft, "stand_turn_ls_", -1, &velocity_turn, PS_STAND, "fx_stand_f", "fx_stand_b",
        "fx_stand_l", "fx_stand_r");
    anim().AddAnim(eAnimStandTurnRight, "stand_turn_rs_", -1, &velocity_turn, PS_STAND, "fx_stand_f", "fx_stand_b",
        "fx_stand_l", "fx_stand_r");
    anim().AddAnim(eAnimWalkFwd, "stand_walk_fwd_", -1, &velocity_walk, PS_STAND, "fx_stand_f", "fx_stand_b",
        "fx_stand_l", "fx_stand_r");
    anim().AddAnim(eAnimRun, "stand_run_fwd_", -1, &velocity_run, PS_STAND, "fx_stand_f", "fx_stand_b", "fx_stand_l",
        "fx_stand_r");
    anim().AddAnim(eAnimAttack, "stand_attack_", -1, &velocity_turn, PS_STAND, "fx_stand_f", "fx_stand_b", "fx_stand_l",
        "fx_stand_r");
    anim().AddAnim(
        eAnimDie, "stand_idle_", 0, &velocity_none, PS_STAND, "fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
    anim().AddAnim(eAnimMiscAction_00, "fall_down_", -1, &velocity_none, PS_STAND, "fx_stand_f", "fx_stand_b",
        "fx_stand_l", "fx_stand_r");
    anim().AddAnim(eAnimMiscAction_01, "fly_", -1, &velocity_none, PS_STAND, "fx_stand_f", "fx_stand_b", "fx_stand_l",
        "fx_stand_r");
    anim().AddAnim(eAnimCheckCorpse, "stand_check_corpse_", -1, &velocity_none, PS_STAND, "fx_stand_f", "fx_stand_b",
        "fx_stand_l", "fx_stand_r");
    anim().AddAnim(
        eAnimEat, "stand_eat_", -1, &velocity_none, PS_STAND, "fx_stand_f", "fx_stand_b", "fx_stand_l", "fx_stand_r");
    anim().AddAnim(eAnimLookAround, "stand_look_around_", -1, &velocity_none, PS_STAND, "fx_stand_f", "fx_stand_b",
        "fx_stand_l", "fx_stand_r");
    anim().AddAnim(eAnimWalkDamaged, "stand_walk_dmg_", -1, &velocity_walk_dmg, PS_STAND, "fx_stand_f", "fx_stand_b",
        "fx_stand_l", "fx_stand_r");
    anim().AddAnim(eAnimRunDamaged, "stand_walk_dmg_", -1, &velocity_run_dmg, PS_STAND, "fx_stand_f", "fx_stand_b",
        "fx_stand_l", "fx_stand_r");

    anim().LinkAction(ACT_STAND_IDLE, eAnimStandIdle);
    anim().LinkAction(ACT_SIT_IDLE, eAnimStandIdle);
    anim().LinkAction(ACT_LIE_IDLE, eAnimStandIdle);
    anim().LinkAction(ACT_WALK_FWD, eAnimWalkFwd);
    anim().LinkAction(ACT_WALK_BKWD, eAnimWalkFwd);
    anim().LinkAction(ACT_RUN, eAnimRun);
    anim().LinkAction(ACT_EAT, eAnimEat);
    anim().LinkAction(ACT_SLEEP, eAnimStandIdle);
    anim().LinkAction(ACT_REST, eAnimStandIdle);
    anim().LinkAction(ACT_DRAG, eAnimStandIdle);
    anim().LinkAction(ACT_ATTACK, eAnimAttack);
    anim().LinkAction(ACT_STEAL, eAnimWalkFwd);
    anim().LinkAction(ACT_LOOK_AROUND, eAnimLookAround);

    READ_IF_EXISTS(pSettings, r_u32, section, "PsyAura_Fake_Delay", 8000);
    READ_IF_EXISTS(pSettings, r_float, section, "PsyAura_Fake_MaxAddDist", 90.f);

    LPCSTR polter_type = pSettings->r_string(section, "type");

    logic_ghost = pSettings->r_bool(section, "logic_ghost");

    if (xr_strcmp(polter_type, "flamer") == 0)
    {
        m_flame = xr_new<CPolterFlame_cs>(this);
        m_flame->load(section);
    }
    else
    {
        m_tele_cs = xr_new<CPolterTele_cs>(this);
        m_tele_cs->load(section);
    }
}

void CPoltergeist_cs::reload(LPCSTR section)
{
    inherited::reload(section);
    Energy::reload(section, "Invisible_");
}

void CPoltergeist_cs::reinit()
{
    inherited::reinit();
    Energy::reinit();

    m_current_position = Position();

    target_height = 0.3f;
    time_height_updated = 0;

    Energy::set_auto_activate();
    Energy::set_auto_deactivate();
    Energy::enable();

    // start hidden
    state_invisible = true;
    setVisible(false);

    m_current_position = Position();
    character_physics_support()->movement()->DestroyCharacter();

    m_height = 0.3f;
    time_height_updated = 0;

    EnableHide();
}

void CPoltergeist_cs::Hide_pcs()
{
    if (state_invisible)
        return;

    state_invisible = true;
    setVisible(false);

    m_current_position = Position();
    character_physics_support()->movement()->DestroyCharacter();

    ability()->on_hide();
}

void CPoltergeist_cs::Show_pcs()
{
    if (!state_invisible)
        return;

    state_invisible = false;

    setVisible(TRUE);

    com_man().seq_run(anim().get_motion_id(eAnimMiscAction_00));

    Position() = m_current_position;
    character_physics_support()->movement()->SetPosition(Position());
    character_physics_support()->movement()->CreateCharacter();

    ability()->on_show();
}

void CPoltergeist_cs::UpdateCL()
{
    inherited::UpdateCL();
    def_lerp(m_height, target_height, HEIGHT_CHANGE_VELOCITY, client_update_fdelta());

    ability()->update_frame();
}

void CPoltergeist_cs::ForceFinalAnimation()
{
    if (state_invisible)
        anim().SetCurAnim(eAnimMiscAction_01);
}

void CPoltergeist_cs::shedule_Update(u32 dt)
{
    inherited::shedule_Update(dt);
    CTelekinesis::schedule_update();
    Energy::schedule_update();

    UpdateHeight();

    ability()->update_schedule();
}

BOOL CPoltergeist_cs::net_Spawn(CSE_Abstract* DC)
{
    if (!inherited::net_Spawn(DC))
        return (FALSE);

    // спаунится нивидимым
    setVisible(false);
    ability()->on_hide();

    return (TRUE);
}

void CPoltergeist_cs::net_Destroy()
{
    inherited::net_Destroy();
    Energy::disable();

    ability()->on_destroy();
}

void CPoltergeist_cs::Die(CObject* who)
{
    // oldSerpski_stalker,
    if (m_tele_cs)
    {
        if (state_invisible)
        {
            setVisible(true);

            if (PPhysicsShell())
            {
                Fmatrix M;
                M.set(XFORM());
                M.translate_over(m_current_position);
                PPhysicsShell()->SetTransform(M, mh_unspecified); // ForserX
            }
            else
                Position() = m_current_position;
        }
    }

    inherited::Die(who);
    Energy::disable();
    CTelekinesis::Deactivate();
    ability()->on_die();
}

void CPoltergeist_cs::Hit(SHit* pHDS)
{
    ability()->on_hit(pHDS);
    inherited::Hit(pHDS);
}

void CPoltergeist_cs::UpdateHeight()
{
    if (!state_invisible)
        return;

    u32 cur_time = Device.dwTimeGlobal;

    if (time_height_updated < cur_time)
    {
        time_height_updated = cur_time + Random.randI(HEIGHT_CHANGE_MIN_TIME, HEIGHT_CHANGE_MAX_TIME);
        target_height = Random.randF(HEIGHT_MIN, HEIGHT_MAX);
    }
}

void CPoltergeist_cs::on_activate()
{
    if (m_disable_hide)
        return;

    Hide_pcs();

    m_height = 2.0f; // Летаем как обычно 2 метра над землей, убрал 0.3f, проваливался под землю
    time_height_updated = 0;
}

void CPoltergeist_cs::on_deactivate()
{
    if (m_disable_hide)
        return;

    Show_pcs();
}

CMovementManager* CPoltergeist_cs::create_movement_manager()
{
    m_movement_manager = xr_new<CPoltergeisMovementManager_cs>(this);

    control().add(m_movement_manager, ControlCom::eControlPath);
    control().install_path_manager(m_movement_manager);
    control().set_base_controller(m_path_base, ControlCom::eControlPath);

    return (m_movement_manager);
}

void CPoltergeist_cs::net_Relcase(CObject* O)
{
    inherited::net_Relcase(O);
    CTelekinesis::remove_links(O);
}


