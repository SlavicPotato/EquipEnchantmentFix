#include "pch.h"

namespace EEF
{
    static EnchantmentEnforcerTask s_eft;
    static PlayerInvWeightRecalcTask s_wrct;

    static bool s_validateOnEffectRemoved;
    static bool s_validateOnLoad;
    static bool s_doRecalcWeight;

    static bool s_triggeredWeightRecalc = false;

    static bool IsREFRValid(TESObjectREFR* a_refr)
    {
        return (!(a_refr->flags & a_refr->kFlagIsDeleted) && !a_refr->IsDead());
    }

    SKMP_FORCEINLINE static EnchantmentItem* GetEnchantment(TESForm* a_form, BaseExtraList* a_extraData)
    {
        EnchantmentItem* enchantment = nullptr;

        auto extraEnchant = static_cast<ExtraEnchantment*>(a_extraData->GetByType(kExtraData_Enchantment));
        if (extraEnchant)
            enchantment = extraEnchant->enchant;

        if (!enchantment)
        {
            auto enchantable = RTTI<TESEnchantableForm>::Cast(a_form);
            if (enchantable)
                return enchantable->enchantment;
        }

        return enchantment;
    }

    SKMP_FORCEINLINE static bool HasItemAbility(Actor* a_actor, TESForm* a_form, EnchantmentItem* a_enchantment)
    {
        auto effects = a_actor->magicTarget.GetActiveEffects();
        if (!effects)
            return false;

        SInt32 i = 0;
        auto effect = effects->GetNthItem(i);

        while (effect)
        {
            if (effect->sourceItem == a_form &&
                effect->item == a_enchantment)
            {
                return true;
            }

            i++;
            effect = effects->GetNthItem(i);
        }

        return false;
    }

    bool EquippedEnchantedItemCollector::Accept(InventoryEntryData* a_entryData)
    {
        if (!a_entryData || !a_entryData->type || a_entryData->countDelta < 1)
            return true;

        if (a_entryData->type->formType != TESObjectARMO::kTypeID)
            return true;

        auto extendDataList = a_entryData->extendDataList;
        if (!extendDataList)
            return true;

        SInt32 i = 0;
        auto extraDataList = extendDataList->GetNthItem(i);

        while (extraDataList)
        {
            if (extraDataList->HasType(kExtraData_WornLeft)
                || extraDataList->HasType(kExtraData_Worn))
            {
                auto enchantment = GetEnchantment(a_entryData->type, extraDataList);
                if (enchantment) {
                    m_results.emplace_back(a_entryData->type, extraDataList, enchantment);
                }

                break;
            }

            i++;
            extraDataList = extendDataList->GetNthItem(i);

        }

        return true;
    }

    bool FindItemVisitor::Accept(InventoryEntryData* a_entryData)
    {
        if (!a_entryData || !a_entryData->type)
            return true;

        if (a_entryData->type->formType != TESObjectARMO::kTypeID)
            return true;

        if (a_entryData->type != m_match)
            return true;

        if (a_entryData->countDelta < 1)
            return true;

        auto extendDataList = a_entryData->extendDataList;
        if (!extendDataList)
            return true;

        SInt32 i = 0;
        auto extraDataList = extendDataList->GetNthItem(i);

        while (extraDataList)
        {
            if (extraDataList->HasType(kExtraData_WornLeft) ||
                extraDataList->HasType(kExtraData_Worn))
            {
                m_result.m_match = true;
                m_result.m_form = a_entryData->type;
                m_result.m_extraData = extraDataList;
                return false;
            }

            i++;
            extraDataList = extendDataList->GetNthItem(i);

        }

        return true;
    }

    SKMP_FORCEINLINE static void ScheduleEFT(UInt32 a_formid) {
        if (s_eft.Add(a_formid))
            SKSE::g_taskInterface->AddTask(&s_eft);
    }

    SKMP_FORCEINLINE static void ClearEFTData(bool a_free = false) {
        if (a_free)
            s_eft.m_data.swap(decltype(s_eft.m_data)());
        else
            s_eft.m_data.clear();
    }

    void EnchantmentEnforcerTask::Run()
    {
        IScopedCriticalSection _(std::addressof(m_lock));

        if (m_data.empty())
            return;

        for (const auto& e : m_data)
        {
            auto actor = e.Lookup<Actor>();
            if (actor == nullptr)
                continue;

            //PruneDupes(actor);
            ProcessActor(actor);
        }

        m_data.clear();
    }

    void EnchantmentEnforcerTask::ProcessActor(Actor* a_actor)
    {
        if (!IsREFRValid(a_actor))
            return;

        auto containerChanges = static_cast<ExtraContainerChanges*>(a_actor->extraData.GetByType(kExtraData_ContainerChanges));
        if (!containerChanges)
            return;

        if (!containerChanges->data ||
            !containerChanges->data->objList)
        {
            return;
        }

        EquippedEnchantedItemCollector collector;
        containerChanges->data->objList->Visit(collector);

        for (auto& e : collector.m_results) {
            if (!HasItemAbility(a_actor, e.m_form, e.m_enchantment)) {
                a_actor->UpdateArmorAbility(e.m_form, e.m_extraList);
            }
        }

    }

    void EEFEventHandler::HandleEvent(TESEquipEvent* a_evn)
    {
        if (!a_evn->equipped)
            return;

        if (a_evn->actor == nullptr)
            return;

        if (!IsREFRValid(a_evn->actor))
            return;

        auto actor = RTTI<Actor>::Cast(a_evn->actor);
        if (!actor)
            return;

        auto form = a_evn->baseObject.Lookup();
        if (!form)
            return;

        if (form->formType != TESObjectARMO::kTypeID)
            return;

        auto containerChanges = static_cast<ExtraContainerChanges*>(actor->extraData.GetByType(kExtraData_ContainerChanges));
        if (!containerChanges)
            return;

        FindItemVisitor visitor(form);
        containerChanges->data->objList->Visit(visitor);

        if (!visitor.m_result.m_match)
            return;

        auto enchantment = GetEnchantment(visitor.m_result.m_form, visitor.m_result.m_extraData);
        if (!enchantment)
            return;

        if (!HasItemAbility(actor, visitor.m_result.m_form, enchantment))
            actor->UpdateArmorAbility(visitor.m_result.m_form, visitor.m_result.m_extraData);
    }

    auto EEFEventHandler::ReceiveEvent(TESEquipEvent* a_evn, EventDispatcher<TESEquipEvent>*)
        -> EventResult
    {
        if (a_evn)
            HandleEvent(a_evn);

        return kEvent_Continue;
    }

    auto EEFEventHandler::ReceiveEvent(TESObjectLoadedEvent* evn, EventDispatcher<TESObjectLoadedEvent>*)
        -> EventResult
    {
        if (evn && evn->loaded)
        {
            auto form = evn->formId.Lookup();
            if (form && form->formType == Actor::kTypeID)
                ScheduleEFT(form->formID);
        }

        return kEvent_Continue;
    }

    auto EEFEventHandler::ReceiveEvent(TESInitScriptEvent* evn, EventDispatcher<TESInitScriptEvent>*)
        -> EventResult
    {
        if (evn && evn->reference)
        {
            if (evn->reference->loadedState &&
                evn->reference->formType == Actor::kTypeID &&
                !evn->reference->IsDead())
            {
                ScheduleEFT(evn->reference->formID);
            }
        }

        return kEvent_Continue;
    }

    static void MessageHandler(SKSEMessagingInterface::Message* a_message)
    {
        switch (a_message->type)
        {
        case SKSEMessagingInterface::kMessage_InputLoaded:
        {
            if (s_validateOnLoad)
            {
                auto edl = GetEventDispatcherList();
                auto handler = EEFEventHandler::GetSingleton();

                edl->objectLoadedDispatcher.AddEventSink(handler);
            }
        }
        break;
        case SKSEMessagingInterface::kMessage_DataLoaded:
        {
            auto edl = GetEventDispatcherList();
            auto handler = EEFEventHandler::GetSingleton();

            edl->equipDispatcher.AddEventSink(handler);
            if (s_validateOnLoad)
                edl->initScriptDispatcher.AddEventSink(handler);
        }
        break;
        case SKSEMessagingInterface::kMessage_PreLoadGame:

            if (s_doRecalcWeight)
                s_triggeredWeightRecalc = false;

        case SKSEMessagingInterface::kMessage_NewGame:
        {
            if (s_validateOnLoad || s_validateOnEffectRemoved)
                ClearEFTData(true);

            break;
        }
        case SKSEMessagingInterface::kMessage_PostLoadGame:

            if (s_doRecalcWeight)
                SKSE::g_taskInterface->AddTask(&s_wrct);
            break;
        }

    }

    void PlayerInvWeightRecalcTask::Run()
    {
        auto player = *g_thePlayer;
        if (!player)
            return;

        auto containerChanges = static_cast<ExtraContainerChanges*>(player->extraData.GetByType(kExtraData_ContainerChanges));
        if (!containerChanges || !containerChanges->data)
            return;

        containerChanges->data->totalWeight = -1.0f;

        s_triggeredWeightRecalc = true;
    }

    static removeActiveEffect_t removeActiveEffect_o;
    static auto removeActiveEffect_addr = IAL::Addr(33743, 0x213);

    static void removeActiveEffect_hook(MagicTarget* target, ActiveEffect* effect, uint8_t unk0)
    {
        if (effect &&
            effect->sourceItem &&
            effect->sourceItem->formType == TESObjectARMO::kTypeID &&
            effect->target &&
            effect->target->MagicTargetIsActor())
        {
            auto actor = RTTI<Actor>()(effect->target);
            if (actor)
            {
                if (!(actor->flags & actor->kFlagIsDeleted))
                {
                    ScheduleEFT(actor->formID);
                }
            }

        }

        removeActiveEffect_o(target, effect, unk0);
    }

    static auto inv_DispelWornItemEnchantsVisitor_addr = IAL::Addr(50212, 0x47B);
    static auto addrem_DispelWornItemEnchantsVisitor_addr = IAL::Addr(24234, 0xE3);

    inv_DispelWornItemEnchantsVisitor_t inv_DispelWornItemEnchantsVisitor_o;

    static void Inventory_DispelWornItemEnchantsVisitor_hook(Character* a_actor)
    {
        if (!IsREFRValid(a_actor))
        {
            inv_DispelWornItemEnchantsVisitor_o(a_actor);
            return;
        }

        auto containerChanges = static_cast<ExtraContainerChanges*>(a_actor->extraData.GetByType(kExtraData_ContainerChanges));
        if (!containerChanges || !containerChanges->data || !containerChanges->data->objList)
        {
            inv_DispelWornItemEnchantsVisitor_o(a_actor);
            return;
        }

        ScheduleEFT(a_actor->formID);

        auto effects = a_actor->magicTarget.GetActiveEffects();
        if (!effects)
            return;

        SInt32 i = 0;
        auto effect = effects->GetNthItem(i);

        while (effect)
        {
            if (effect->sourceItem && effect->item &&
                !(effect->flags & effect->kFlag_Dispelled))
            {
                FindItemVisitor visitor(effect->sourceItem);
                containerChanges->data->objList->Visit(visitor);

                if (!visitor.m_result.m_match) {
                    effect->Dispel(false);
                }
            }

            i++;
            effect = effects->GetNthItem(i);
        }


    }

    static auto updateArmorAbility1_addr = IAL::Addr(36976, 0x3BB);

    updateArmorAbility_t updateArmorAbility_o;

    static void UpdateArmorAbility_Hook1(Actor* a_actor, TESForm* a_form, BaseExtraList* a_extraData)
    {
        if (a_actor && a_form && a_extraData)
        {
            if (a_form->formType == TESObjectARMO::kTypeID)
            {
                auto enchantment = GetEnchantment(a_form, a_extraData);
                if (enchantment)
                {
                    if (HasItemAbility(a_actor, a_form, enchantment))
                    {
                        //_DMESSAGE("%X: suppressing update: %X, %X, %s", a_actor->formID, a_form->formID, enchantment->formID, enchantment->fullName.GetName());
                        return;
                    }
                }
            }
        }

        updateArmorAbility_o(a_actor, a_form, a_extraData);
    }

    bool Initialize()
    {
        INIReader confReader;

        confReader.Load(PLUGIN_INI_FILE);

        int r = confReader.ParseError();
        if (r != 0)
            gLog.Warning("Unable to load the configuration file, using defaults (%d)", r);

        s_validateOnEffectRemoved = confReader.Get("EEF", "OnEffectRemoved", true);
        s_validateOnLoad = confReader.Get("EEF", "OnActorLoad", true);
        s_doRecalcWeight = confReader.Get("EEF", "RecalcPlayerInventoryWeightOnLoad", false);
        bool redirectDispelWornItemEnchantsVisitor = confReader.Get("EEF", "RedirectDispelWornItemEnchantsVisitor", true);

        if (s_validateOnEffectRemoved) {
            if (!Hook::Call5(removeActiveEffect_addr, uintptr_t(removeActiveEffect_hook), removeActiveEffect_o))
                gLog.Warning("Failed to install active effect update hook, NPC inventory fix won't work");
            else
                gLog.Message("OnEffectRemoved ON");
        }

        if (redirectDispelWornItemEnchantsVisitor)
        {
            inv_DispelWornItemEnchantsVisitor_t tmpa, tmpb;

            bool ha = Hook::Call5(
                inv_DispelWornItemEnchantsVisitor_addr,
                uintptr_t(Inventory_DispelWornItemEnchantsVisitor_hook),
                tmpa);

            if (!ha)
            {
                gLog.Warning("DispelWornItemEnchantsVisitor (inventory) failed");
            }

            bool hb = Hook::Call5(
                addrem_DispelWornItemEnchantsVisitor_addr,
                uintptr_t(Inventory_DispelWornItemEnchantsVisitor_hook),
                tmpb);

            if (!hb)
            {
                gLog.Warning("DispelWornItemEnchantsVisitor (add/remove) failed");
            }

            if (ha && hb) {
                ASSERT(tmpa == tmpb);
                inv_DispelWornItemEnchantsVisitor_o = tmpa;
            }
            else {
                inv_DispelWornItemEnchantsVisitor_o = ha ? tmpa : tmpb;
            }

            if (ha || hb)
                gLog.Message("DispelWornItemEnchantsVisitor ON");

            if (!Hook::Call5(
                updateArmorAbility1_addr,
                uintptr_t(UpdateArmorAbility_Hook1),
                updateArmorAbility_o))
            {
                gLog.Error("UpdateArmorAbility hook failed");
            }

        }

        if (s_validateOnLoad)
            gLog.Message("OnActorLoad ON");

        bool res = SKSE::g_messaging->RegisterListener(SKSE::g_pluginHandle, "SKSE", MessageHandler);

        if (!res)
            gLog.FatalError("Couldn't add message listener");

        return res;
    }
}