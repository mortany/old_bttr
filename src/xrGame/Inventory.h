#pragma once
#include "inventory_item.h"

class CInventory;
class CInventoryItem;
class CHudItem;
class CInventoryOwner;

class CInventorySlot
{									
public:
							CInventorySlot		();
	virtual					~CInventorySlot		();

	bool					CanBeActivated		() const;

	PIItem					m_pIItem;
	bool					m_bPersistent;
	bool					m_bAct;
};

typedef xr_vector<CInventorySlot> TISlotArr;


class CInventory
{				
public:
							CInventory			();
	virtual					~CInventory			();

	float 					TotalWeight			() const;
	float 					CalcTotalWeight		();

	void					Take				(CGameObject *pObj, bool bNotActivate, bool strict_placement);
	//if just_before_destroy is true, then activate will be forced (because deactivate message will not deliver)
	bool					DropItem			(CGameObject *pObj, bool just_before_destroy, bool dont_create_shell);
	void					Clear				();

	u16 					m_last_slot;
	IC u16					FirstSlot			() const {return KNIFE_SLOT;}
	IC u16					LastSlot			() const {return m_last_slot;} // not "end"
	IC bool					SlotIsPersistent	(u16 slot_id) const {return m_slots[slot_id].m_bPersistent;}
	bool					Slot				(u16 slot_id, PIItem pIItem, bool bNotActivate = false, bool strict_placement=false);	
	bool					Belt				(PIItem pIItem, bool strict_placement=false);
	bool					Ruck				(PIItem pIItem, bool strict_placement=false);

	bool 					InSlot				(const CInventoryItem* pIItem) const;
	bool 					InBelt				(const CInventoryItem* pIItem) const;
	bool 					InRuck				(const CInventoryItem* pIItem) const;

	bool 					CanPutInSlot		(PIItem pIItem, u16 slot_id) const;
	bool 					CanPutInBelt		(PIItem pIItem);
	bool 					CanPutInRuck		(PIItem pIItem) const;

	bool					CanTakeItem			(CInventoryItem *inventory_item) const;

	void					Activate			(u16 slot, /*EActivationReason reason=eGeneral, */bool bForce=false);
	void					ActivateDeffered();
	PIItem					GetNextActiveGrenade();
	bool					ActivateNextGrenage();

	PIItem					ActiveItem			()const					{return (m_iActiveSlot==NO_ACTIVE_SLOT)?NULL:ItemFromSlot(m_iActiveSlot);}
	PIItem					ItemFromSlot		(u16 slot) const;

	bool					Action				(u16 cmd, u32 flags);
	void					ActiveWeapon		(u16 slot);
	void					Update				();
	// ���� �� ����� ����������� IItem
	PIItem					Same				(const PIItem pIItem, bool bSearchRuck) const;
	// ���� �� ����� IItem ��� ���������� �����
	PIItem					SameSlot			(const u16 slot, PIItem pIItem, bool bSearchRuck) const;
	// ���� �� ����� ��� � ������� IItem � ��������� ������ (cName())
	PIItem					Get					(LPCSTR name, bool bSearchRuck) const;
	// ���� �� ����� ��� � ������� IItem � ��������� ������ (id)
	PIItem					Get					(const u16  id,	 bool bSearchRuck) const;
	// ���� �� ����� ��� � ������� IItem � ��������� CLS_ID
	PIItem					Get					(CLASS_ID cls_id,  bool bSearchRuck) const;
	PIItem					GetAny				(LPCSTR name) const;//search both (ruck and belt)
	PIItem					GetAmmoOnBelt		(LPCSTR name) const;
	PIItem					item				(CLASS_ID cls_id) const;
	
	// get all the items with the same section name
	virtual u32				dwfGetSameItemCount	(LPCSTR caSection, bool SearchAll = false);	
	virtual u32				dwfGetGrenadeCount	(LPCSTR caSection, bool SearchAll);	
	// get all the items with the same object id
	virtual bool			bfCheckForObject	(ALife::_OBJECT_ID tObjectID);	
	PIItem					get_object_by_id	(ALife::_OBJECT_ID tObjectID);

	u32						dwfGetObjectCount	();
	PIItem					tpfGetObjectByIndex	(int iIndex);
	PIItem					GetItemFromInventory(LPCSTR caItemName);

	bool					Eat					(PIItem pIItem);
	bool					ClientEat			(PIItem pIItem);

	IC u16					GetActiveSlot		() const			{return m_iActiveSlot;}
	
	void					SetPrevActiveSlot	(u16 ActiveSlot)	{m_iPrevActiveSlot = ActiveSlot;}
	u16						GetPrevActiveSlot	() const			{return m_iPrevActiveSlot;}
	IC u16					GetNextActiveSlot	() const			{return m_iNextActiveSlot;}

	void					SetActiveSlot		(u16 ActiveSlot)	{m_iActiveSlot = m_iNextActiveSlot = ActiveSlot; }

	bool 					IsSlotsUseful		() const			{return m_bSlotsUseful;}	 
	void 					SetSlotsUseful		(bool slots_useful) {m_bSlotsUseful = slots_useful;}
	bool 					IsBeltUseful		() const			{return m_bBeltUseful;}
	void 					SetBeltUseful		(bool belt_useful)	{m_bBeltUseful = belt_useful;}

	void					SetSlotsBlocked		(u16 mask, bool bBlock);
	
	void					BlockSlot(u16 slot_id);
	void					UnblockSlot(u16 slot_id);
	bool					IsSlotBlocked(PIItem const iitem) const;
	
	bool					m_block_hands;
	void					BlockHands(bool hands_only) { m_block_hands = hands_only; };
	bool					IsHandsBlocked() { return m_block_hands; };
	
	TIItemContainer			m_all;
	TIItemContainer			m_ruck, m_belt;
	TIItemContainer			m_activ_last_items;

protected:
	TISlotArr				m_slots;
public:
	//���������� ��� ����� PDA � ����� � �����
	void				AddAvailableItems			(TIItemContainer& items_container, bool for_trade, bool bOverride = false) const;

	float				GetMaxWeight				() const				{return m_fMaxWeight;}
	void				SetMaxWeight				(float weight)			{m_fMaxWeight = weight;}

	u32					BeltWidth					() const;

	inline	CInventoryOwner*GetOwner				() const				{ return m_pOwner; }
	

	friend class CInventoryOwner;


	u32					ModifyFrame					() const					{ return m_dwModifyFrame; }
	void				InvalidateState				()							{ m_dwModifyFrame = Device.dwFrame; }
	void				Items_SetCurrentEntityHud	(bool current_entity);
	bool				isBeautifulForActiveSlot	(CInventoryItem *pIItem);
protected:
	void					UpdateDropTasks		();
	void					UpdateDropItem		(PIItem pIItem);

	// �������� ���� � ���� ������� ������ �������� ����� �����
    //�������� ��������� � ������� ��������� (��� ����� ������)
	u16 				m_iActiveSlot;
	u16 				m_iNextActiveSlot;
	u16 				m_iPrevActiveSlot;

	CInventoryOwner*	m_pOwner;

	//����, ������������ ������� ����� � ���������
	bool				m_bBeltUseful;
	//����, ����������� ������������� ������
	bool				m_bSlotsUseful;

	// ������������ ��� ���������
	float				m_fMaxWeight;
	// ������� ��� � ���������
	float				m_fTotalWeight;

	//���� �� ������� ��������� ��������� ��������� � ���������
	u32					m_dwModifyFrame;

	bool				m_drop_last_frame;
	bool				m_change_after_deactivate;

	void				SendActionEvent		(u16 cmd, u32 flags);

private:
	std::vector<u8> m_blocked_slots;
	bool				IsSlotBlocked(u16 slot_id) const;
	void				TryActivatePrevSlot		();
	void				TryDeactivateActiveSlot	();
};