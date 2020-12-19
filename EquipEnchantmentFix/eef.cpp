#include "pch.h"

namespace EEF
{
    static EnchantmentEnforcerTask s_eft;

    static bool s_validateOnEffectRemoved;
    static bool s_validateOnLoad;

    SKMP_FORCEINLINE static EnchantmentItem* GetEnchantment(TESForm* a_form, BaseExtraList* a_extraData)
    {
        EnchantmentItem* enchantment = nullptr;

        auto extraEnchant = static_cast<ExtraEnchantment*>(a_extraData->GetByType(kExtraData_Enchantment));
        if (extraEnchant)
            enchantment = extraEnchant->enchant;

        if (!enchantment)
        {
            auto enchantable = DYNAMIC_CAST(a_form, TESForm, TESEnchantableForm);
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

        auto numEffects = effects->Count();
        for (decltype(numEffects) i = 0; i < numEffects; i++)
        {
            auto effect = effects->GetNthItem(i);
            if (!effect)
                continue;

            if (effect->sourceItem != a_form)
                continue;

            if (effect->item == a_enchantment)
                return true;
        }

        return false;
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

        for (const auto e : m_data)
        {
            auto form = LookupFormByID(e);
            if (!form)
                continue;

            if (form->formType != Actor::kTypeID)
                continue;

            auto actor = DYNAMIC_CAST(form, TESForm, Actor);
            if (actor == nullptr)
                continue;

            ProcessActor(actor);
        }

        m_data.clear();
    }

    void EnchantmentEnforcerTask::ProcessActor(Actor* a_actor)
    {
        struct ArmorEntry
        {
            ArmorEntry(
                TESForm* a_form,
                BaseExtraList* a_extraList,
                EnchantmentItem* a_enchantment
            ) :
                m_form(a_form),
                m_extraList(a_extraList),
                m_enchantment(a_enchantment)
            {}

            TESForm* m_form;
            BaseExtraList* m_extraList;
            EnchantmentItem* m_enchantment;
        };

        struct EquippedArmorCollector
        {
            bool Accept(InventoryEntryData* a_entryData)
            {
                if (!a_entryData || !a_entryData->type || a_entryData->type->formType != TESObjectARMO::kTypeID)
                    return true;

                auto extendDataList = a_entryData->extendDataList;
                if (!extendDataList)
                    return true;

                for (auto it = extendDataList->Begin(); !it.End(); ++it)
                {
                    auto extraDataList = it.Get();

                    if (!extraDataList)
                        continue;

                    if (!extraDataList->HasType(kExtraData_Worn) &&
                        !extraDataList->HasType(kExtraData_WornLeft))
                    {
                        continue;
                    }

                    auto enchantment = GetEnchantment(a_entryData->type, extraDataList);
                    if (!enchantment)
                        break;

                    m_results.emplace_back(a_entryData->type, extraDataList, enchantment);
                }

                return true;
            }

            stl::vector<ArmorEntry> m_results;
        };

        if (!a_actor->loadedState || a_actor->IsDead())
            return;

        //_DMESSAGE("%s: %.8X (%s)", __FUNCTION__, e, CALL_MEMBER_FN(actor, GetReferenceName)());

        auto containerChanges = static_cast<ExtraContainerChanges*>(a_actor->extraData.GetByType(kExtraData_ContainerChanges));
        if (!containerChanges)
            return;

        if (!containerChanges->data ||
            !containerChanges->data->objList)
        {
            return;
        }

        EquippedArmorCollector collector;
        containerChanges->data->objList->Visit(collector);

        for (auto& e : collector.m_results)
            if (!HasItemAbility(a_actor, e.m_form, e.m_enchantment))
                CALL_MEMBER_FN(a_actor, UpdateArmorAbility)(e.m_form, e.m_extraList);

    }

    void EEFEventHandler::HandleEvent(TESEquipEvent* a_evn)
    {

        if (!a_evn->equipped)
            return;

        if (a_evn->actor == nullptr)
            return;

        auto actor = DYNAMIC_CAST(a_evn->actor, TESObjectREFR, Actor);
        if (!actor)
            return;

        if (!actor->loadedState || actor->IsDead())
            return;

        auto form = LookupFormByID(a_evn->baseObject);
        if (!form)
            return;

        if (form->formType != TESObjectARMO::kTypeID)
            return;

        auto containerChanges = static_cast<ExtraContainerChanges*>(actor->extraData.GetByType(kExtraData_ContainerChanges));
        if (!containerChanges)
            return;

        MatchForm matcher(form);

        auto equipData = containerChanges->FindEquipped(matcher);
        if (!equipData.pForm || !equipData.pExtraData)
            return;

        auto enchantment = GetEnchantment(equipData.pForm, equipData.pExtraData);
        if (!enchantment)
            return;

        if (!HasItemAbility(actor, equipData.pForm, enchantment))
            CALL_MEMBER_FN(actor, UpdateArmorAbility)(equipData.pForm, equipData.pExtraData);
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
            auto form = LookupFormByID(evn->formId);
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
        case SKSEMessagingInterface::kMessage_NewGame:
        case SKSEMessagingInterface::kMessage_PreLoadGame:
        {
            if (s_validateOnLoad || s_validateOnEffectRemoved)
                ClearEFTData(true);
        }
        break;
        }

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
            auto actor = DYNAMIC_CAST(effect->target, MagicTarget, Actor);
            if (actor)
            {
                if (actor->loadedState &&
                    !actor->IsDead())
                {
                    ScheduleEFT(actor->formID);
                }
            }

        }

        removeActiveEffect_o(target, effect, unk0);
    }

    static auto inv_DispelWornItemEnchantsVisitor_addr = IAL::Addr(50212, 0x47B);
    static auto addrem_DispelWornItemEnchantsVisitor_addr = IAL::Addr(24234, 0xE3);

    typedef void(__cdecl* inv_DispelWornItemEnchantsVisitor_t)(Actor* a_actor);

    inv_DispelWornItemEnchantsVisitor_t inv_DispelWornItemEnchantsVisitor_o;

    static void Inventory_DispelWornItemEnchantsVisitor_hook(Character* a_actor)
    {
        /*if (!a_actor)
            return;*/

            //_DMESSAGE("%s (%X | %s)", __FUNCTION__, a_actor->formID, CALL_MEMBER_FN(a_actor, GetReferenceName)());

        if (a_actor->IsDead())
        {
            inv_DispelWornItemEnchantsVisitor_o(a_actor);
            return;
        }

        auto containerChanges = static_cast<ExtraContainerChanges*>(a_actor->extraData.GetByType(kExtraData_ContainerChanges));
        if (!containerChanges) {
            inv_DispelWornItemEnchantsVisitor_o(a_actor);
            return;
        }

        auto effects = a_actor->magicTarget.GetActiveEffects();
        if (!effects)
            return;

        auto numEffects = effects->Count();
        for (decltype(numEffects) i = 0; i < numEffects; i++)
        {
            auto effect = effects->GetNthItem(i);
            if (!effect)
                continue;

            if (!effect->sourceItem || !effect->item)
                continue;

            if (effect->flags & ActiveEffect::kFlag_Dispelled)
                continue;

            MatchForm matcher(effect->sourceItem);

            auto equipData = containerChanges->FindEquipped(matcher);
            if (!equipData.pForm && !equipData.pExtraData)
                CALL_MEMBER_FN(effect, Dispel)(false);
        }

    }

    bool Initialize()
    {
        g_confReader.Load(PLUGIN_INI_FILE);

        int r = g_confReader.ParseError();
        if (r != 0)
            gLog.Warning("Unable to load the configuration file, using defaults (%d)", r);

        s_validateOnEffectRemoved = g_confReader.GetBoolean("EEF", "OnEffectRemoved", true);
        s_validateOnLoad = g_confReader.GetBoolean("EEF", "OnActorLoad", true);
        bool redirectDispelWornItemEnchantsVisitor = g_confReader.GetBoolean("EEF", "RedirectDispelWornItemEnchantsVisitor", true);

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
        }

        if (s_validateOnLoad)
            gLog.Message("OnActorLoad ON");

        bool res = SKSE::g_messaging->RegisterListener(SKSE::g_pluginHandle, "SKSE", MessageHandler);

        if (!res)
            gLog.FatalError("Couldn't add message listener");

        return res;
    }
}