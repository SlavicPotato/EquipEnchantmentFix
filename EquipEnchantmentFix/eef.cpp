#include "pch.h"

namespace EEF
{
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

    void EquipEventHandler::HandleEvent(TESEquipEvent* a_evn)
    {
        class MatchForm :
            public FormMatcher
        {
        public:
            MatchForm(TESForm* a_form) :
                m_form(a_form)
            {
            }

            virtual bool Matches(TESForm* a_form) const {
                return m_form == a_form;
            }

        private:

            TESForm* m_form;
        };

        if (!a_evn->equipped)
            return;

        if (a_evn->actor == nullptr)
            return;

        auto actor = DYNAMIC_CAST(a_evn->actor, TESObjectREFR, Actor);
        if (!actor)
            return;

        auto form = LookupFormByID(a_evn->baseObject);
        if (!form)
            return;

        if (form->formType != TESObjectARMO::kTypeID)
            return;

        auto containerChanges = static_cast<ExtraContainerChanges*>(actor->extraData.GetByType(kExtraData_ContainerChanges));
        if (!containerChanges)
            return;

        auto equipData = containerChanges->FindEquipped(MatchForm(form));
        if (!equipData.pForm || !equipData.pExtraData)
            return;

        auto enchantment = GetEnchantment(equipData.pForm, equipData.pExtraData);
        if (!enchantment)
            return;

        if (!HasItemAbility(actor, equipData.pForm, enchantment))
            CALL_MEMBER_FN(actor, UpdateArmorAbility)(equipData.pForm, equipData.pExtraData);
    }

    auto EquipEventHandler::ReceiveEvent(TESEquipEvent* a_evn, EventDispatcher<TESEquipEvent>*)
        -> EventResult
    {
        if (a_evn)
            HandleEvent(a_evn);

        return kEvent_Continue;
    }

    static void MessageHandler(SKSEMessagingInterface::Message* a_message)
    {
        if (a_message->type == SKSEMessagingInterface::kMessage_DataLoaded)
        {
            auto edl = GetEventDispatcherList();
            edl->equipDispatcher.AddEventSink(EquipEventHandler::GetSingleton());
        }
    }

    bool Initialize()
    {
        bool res = SKSE::g_messaging->RegisterListener(SKSE::g_pluginHandle, "SKSE", MessageHandler);

        if (!res)
            gLog.FatalError("Couldn't add message listener");

        return res;
    }
}