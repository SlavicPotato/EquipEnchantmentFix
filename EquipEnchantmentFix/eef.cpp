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
		return a_refr &&
		       !a_refr->IsDeleted() &&
		       !a_refr->IsDead();
	}

	static EnchantmentItem* GetEnchantment(
		TESForm* a_form,
		BaseExtraList* a_extraData)
	{
		if (auto extraEnchant = a_extraData->Get<ExtraEnchantment>())
		{
			return extraEnchant->enchant;
		}
		else
		{
			return nullptr;
		}
	}

	static bool HasItemAbility(
		Actor* a_actor,
		TESForm* a_form,
		EnchantmentItem* a_enchantment)
	{
		auto effects = a_actor->GetActiveEffectList();
		if (!effects)
			return false;

		for (auto& effect : *effects)
		{
			if (!effect)
			{
				continue;
			}

			if (effect->source == a_form &&
			    effect->spell == a_enchantment)
			{
				return true;
			}
		}

		return false;
	}

	bool EquippedEnchantedArmorItemCollector::Accept(InventoryEntryData* a_entryData)
	{
		if (!a_entryData || !a_entryData->type)
			return true;

		if (a_entryData->type->formType != TESObjectARMO::kTypeID)
			return true;

		auto extendDataList = a_entryData->extendDataList;
		if (!extendDataList)
			return true;

		for (auto it = extendDataList->Begin(); !it.End(); ++it)
		{
			auto extraList = *it;
			if (!extraList)
			{
				continue;
			}

			{
				BSReadLocker locker(extraList->m_lock);

				auto presence = extraList->m_presence;
				if (!presence)
				{
					continue;
				}

				if (!presence->HasType(ExtraWorn::EXTRA_DATA) &&
				    !presence->HasType(ExtraWornLeft::EXTRA_DATA))
				{
					continue;
				}
			}

			if (auto extraEnchant = extraList->Get<ExtraEnchantment>())
			{
				if (extraEnchant->enchant)
				{
					m_results.emplace_back(a_entryData->type, extraList, extraEnchant->enchant);
				}
			}
		}

		return true;
	}

	bool FindEquippedArmorItemVisitor::Accept(InventoryEntryData* a_entryData)
	{
		if (!a_entryData || !a_entryData->type)
			return true;

		if (a_entryData->type != m_match)
			return true;

		if (a_entryData->type->formType != TESObjectARMO::kTypeID)
			return false;

		auto extendDataList = a_entryData->extendDataList;
		if (!extendDataList)
			return false;

		for (auto it = extendDataList->Begin(); !it.End(); ++it)
		{
			auto extraList = *it;
			if (!extraList)
			{
				continue;
			}

			BSReadLocker locker(extraList->m_lock);

			auto presence = extraList->m_presence;
			if (!presence)
			{
				continue;
			}

			if (presence->HasType(ExtraWorn::EXTRA_DATA) ||
			    presence->HasType(ExtraWornLeft::EXTRA_DATA))
			{
				m_result.m_match = true;
				m_result.m_form = a_entryData->type;
				m_result.m_extraData = extraList;
			}
		}

		return false;
	}

	bool EquipItemHookVisitor::Accept(InventoryEntryData* a_entryData)
	{
		if (!a_entryData || !a_entryData->type)
			return true;

		if (a_entryData->type != m_match)
			return true;

		auto extendDataList = a_entryData->extendDataList;
		if (!extendDataList)
		{
			return false;
		}

		m_result.m_match = true;

		auto it = extendDataList->Begin();

		if (!it.End())
		{
			m_result.m_extraData = *it;
		}

		for (; !it.End(); ++it)
		{
			auto extraList = *it;
			if (!extraList)
			{
				continue;
			}

			BSReadLocker locker(extraList->m_lock);

			auto presence = extraList->m_presence;
			if (!presence)
			{
				continue;
			}

			if (presence->HasType(ExtraWorn::EXTRA_DATA) ||
			    presence->HasType(ExtraWornLeft::EXTRA_DATA))
			{
				m_result.m_equipped = true;
				break;
			}
		}

		return false;
	}

	static void ScheduleEFT(TESObjectREFR* a_ref)
	{
		auto handle = a_ref->GetHandle();
		if (!handle || !handle.IsValid())
		{
			return;
		}

		if (s_eft.Add(handle))
		{
			ISKSE::GetSingleton().GetInterface<SKSETaskInterface>()->AddTask(&s_eft);
		}
	}

	static void ClearEFTData()
	{
		s_eft.m_data.clear();
	}

	void EnchantmentEnforcerTask::Run()
	{
		stl::scoped_lock lock(m_lock);

		if (m_data.empty())
			return;

		for (const auto& e : m_data)
		{
			NiPointer<TESObjectREFR> ref;
			if (e.Lookup(ref))
			{
				if (auto actor = ref->As<Actor>())
				{
					ProcessActor(actor);
				}
			}
		}

		m_data.clear();
	}

	void EnchantmentEnforcerTask::ProcessActor(Actor* a_actor)
	{
		if (!IsREFRValid(a_actor))
			return;

		auto containerChanges = a_actor->extraData.Get<ExtraContainerChanges>();
		if (!containerChanges)
			return;

		if (!containerChanges->data ||
		    !containerChanges->data->objList)
		{
			return;
		}

		EquippedEnchantedArmorItemCollector collector;
		containerChanges->data->objList->Visit(collector);

		for (auto& e : collector.m_results)
		{
			if (!HasItemAbility(a_actor, e.m_form, e.m_enchantment))
			{
				a_actor->UpdateArmorAbility(e.m_form, e.m_extraList);
			}
		}
	}

	void EEFEventHandler::HandleEvent(const TESEquipEvent* a_evn)
	{
		if (!a_evn->equipped)
			return;

		if (a_evn->actor == nullptr)
			return;

		if (!IsREFRValid(a_evn->actor))
			return;

		auto actor = a_evn->actor->As<Actor>();
		if (!actor)
			return;

		auto form = a_evn->baseObject.Lookup();
		if (!form)
			return;

		if (form->formType != TESObjectARMO::kTypeID)
			return;

		auto containerChanges = actor->extraData.Get<ExtraContainerChanges>();
		if (!containerChanges || !containerChanges->data || !containerChanges->data->objList)
			return;

		FindEquippedArmorItemVisitor visitor(form);
		containerChanges->data->objList->Visit(visitor);

		if (!visitor.m_result.m_match || !visitor.m_result.m_extraData)
			return;

		auto enchantment = GetEnchantment(visitor.m_result.m_form, visitor.m_result.m_extraData);
		if (!enchantment)
			return;

		if (!HasItemAbility(actor, visitor.m_result.m_form, enchantment))
			actor->UpdateArmorAbility(visitor.m_result.m_form, visitor.m_result.m_extraData);
	}

	auto EEFEventHandler::ReceiveEvent(const TESEquipEvent* a_evn, BSTEventSource<TESEquipEvent>*)
		-> EventResult
	{
		if (a_evn)
		{
			HandleEvent(a_evn);
		}

		return EventResult::kContinue;
	}

	auto EEFEventHandler::ReceiveEvent(const TESObjectLoadedEvent* evn, BSTEventSource<TESObjectLoadedEvent>*)
		-> EventResult
	{
		if (evn && evn->loaded)
		{
			if (auto actor = evn->formId.As<Actor>())
			{
				ScheduleEFT(actor);
			}
		}

		return EventResult::kContinue;
	}

	auto EEFEventHandler::ReceiveEvent(const TESInitScriptEvent* evn, BSTEventSource<TESInitScriptEvent>*)
		-> EventResult
	{
		if (evn && evn->reference)
		{
			if (evn->reference->loadedState &&
			    evn->reference->formType == Actor::kTypeID &&
			    !evn->reference->IsDead())
			{
				ScheduleEFT(evn->reference);
			}
		}

		return EventResult::kContinue;
	}

	static void MessageHandler(SKSEMessagingInterface::Message* a_message)
	{
		switch (a_message->type)
		{
		case SKSEMessagingInterface::kMessage_InputLoaded:
			{
				if (s_validateOnLoad)
				{
					auto handler = EEFEventHandler::GetSingleton();

					ScriptEventSourceHolder::GetSingleton()->AddEventSink<TESObjectLoadedEvent>(handler);
				}
			}
			break;
		case SKSEMessagingInterface::kMessage_DataLoaded:
			{
				auto edl = ScriptEventSourceHolder::GetSingleton();
				auto handler = EEFEventHandler::GetSingleton();

				edl->AddEventSink<TESEquipEvent>(handler);
				if (s_validateOnLoad)
					edl->AddEventSink<TESInitScriptEvent>(handler);
			}
			break;
		case SKSEMessagingInterface::kMessage_PreLoadGame:

			if (s_doRecalcWeight)
				s_triggeredWeightRecalc = false;

			// fall-through

		case SKSEMessagingInterface::kMessage_NewGame:

			if (s_validateOnLoad || s_validateOnEffectRemoved)
				ClearEFTData();

			break;

		case SKSEMessagingInterface::kMessage_PostLoadGame:

			if (s_doRecalcWeight)
				ISKSE::GetSingleton().GetInterface<SKSETaskInterface>()->AddTask(&s_wrct);

			break;
		}
	}

	void PlayerInvWeightRecalcTask::Run()
	{
		auto player = *g_thePlayer;
		if (!player)
			return;

		auto containerChanges = player->extraData.Get<ExtraContainerChanges>();
		if (!containerChanges || !containerChanges->data)
			return;

		containerChanges->data->totalWeight = -1.0f;

		s_triggeredWeightRecalc = true;
	}

	static auto inv_DispelWornItemEnchantsVisitor_addr = IAL::Addr(50212, 51141, 0x47B, 0x57D);
	static auto addrem_DispelWornItemEnchantsVisitor_addr = IAL::Addr(24234, 24738, 0xE3, 0xC2);

	inv_DispelWornItemEnchantsVisitor_t inv_DispelWornItemEnchantsVisitor_o;
	inv_DispelWornItemEnchantsVisitor_t addrem_DispelWornItemEnchantsVisitor_o;

	static bool Inventory_DispelWornItemEnchantsVisitor_Impl(Character* a_actor)
	{
		if (!IsREFRValid(a_actor))
		{
			return false;
		}

		auto containerChanges = a_actor->extraData.Get<ExtraContainerChanges>();
		if (!containerChanges ||
		    !containerChanges->data ||
		    !containerChanges->data->objList)
		{
			return false;
		}

		ScheduleEFT(a_actor);

		auto effects = a_actor->GetActiveEffectList();
		if (!effects)
			return true;

		for (auto& effect : *effects)
		{
			if (!effect)
			{
				continue;
			}

			if (effect->source &&
			    effect->spell &&
			    !(effect->flags.test(ActiveEffect::Flag::kDispelled)))
			{
				FindEquippedArmorItemVisitor visitor(effect->source);
				containerChanges->data->objList->Visit(visitor);

				if (!visitor.m_result.m_match)
				{
					effect->Dispel(false);
				}
			}
		}

		return true;
	}

	static void Inventory_DispelWornItemEnchantsVisitor_inv_Hook(Character* a_actor)
	{
		if (!Inventory_DispelWornItemEnchantsVisitor_Impl(a_actor))
		{
			inv_DispelWornItemEnchantsVisitor_o(a_actor);
		}
	}

	static void Inventory_DispelWornItemEnchantsVisitor_addrem_Hook(Character* a_actor)
	{
		if (!Inventory_DispelWornItemEnchantsVisitor_Impl(a_actor))
		{
			addrem_DispelWornItemEnchantsVisitor_o(a_actor);
		}
	}

	static auto updateArmorAbility1_addr = IAL::Addr(36976, 38001, 0x3BB, 0x36D);

	updateArmorAbility_t updateArmorAbility_o;

	static EnchantmentItem* GetEnchantmentWithBase(
		TESForm* a_form,
		BaseExtraList* a_extraData)
	{
		EnchantmentItem* enchantment = nullptr;

		if (a_extraData)
		{
			if (auto extraEnchant = a_extraData->Get<ExtraEnchantment>())
			{
				enchantment = extraEnchant->enchant;
			}
		}

		if (!enchantment)
		{
			if (auto enchantable = RTTI<TESEnchantableForm>()(a_form))
			{
				return enchantable->formEnchanting;
			}
		}

		return enchantment;
	}

	static void UpdateArmorAbility_Hook1(Actor* a_actor, TESForm* a_form, BaseExtraList* a_extraData)
	{
		if (a_actor && a_form)
		{
			if (a_form->formType == TESObjectARMO::kTypeID)
			{
				if (auto enchantment = GetEnchantmentWithBase(a_form, a_extraData))
				{
					if (HasItemAbility(a_actor, a_form, enchantment))
					{
						return;
					}
				}
			}
		}

		updateArmorAbility_o(a_actor, a_form, a_extraData);
	}

	static void EquipItem_Hook(
		EquipManager* a_equipManager,
		Actor* a_actor,
		TESForm* a_form,
		BaseExtraList* a_extraList,
		std::int32_t a_count,
		BGSEquipSlot* a_equipSlot,
		bool a_withEquipSound,
		bool a_preventUnequip,
		bool a_showMsg,
		void* a_unk);

	decltype(&EquipItem_Hook) EquipItem_o;
	auto EquipItem_a = IAL::Addr<std::uintptr_t>(37938, 38894, 0x9, 0x9);

	static void EquipItem_Hook(
		EquipManager* a_equipManager,
		Actor* a_actor,
		TESForm* a_form,
		BaseExtraList* a_extraList,
		std::int32_t a_count,
		BGSEquipSlot* a_equipSlot,
		bool a_withEquipSound,
		bool a_preventUnequip,
		bool a_showMsg,
		void* a_unk)
	{
		if (!a_extraList && a_actor && a_form && a_count > 0 && a_actor->processManager)
		{
			if (a_form != a_actor->processManager->equippedObject[0] &&
			    a_form != a_actor->processManager->equippedObject[1])
			{
				auto containerChanges = a_actor->extraData.Get<ExtraContainerChanges>();
				if (containerChanges &&
				    containerChanges->data &&
				    containerChanges->data->objList)
				{
					EquipItemHookVisitor v(a_form);
					containerChanges->data->objList->Visit(v);

					if (v.m_match && !v.m_result.m_equipped)
					{
						a_extraList = v.m_result.m_extraData;
					}
				}
			}
		}

		EquipItem_o(
			a_equipManager,
			a_actor,
			a_form,
			a_extraList,
			a_count,
			a_equipSlot,
			a_withEquipSound,
			a_preventUnequip,
			a_showMsg,
			a_unk);
	}

	bool Initialize()
	{
		INIConfReader confReader(PLUGIN_INI_FILE_NOEXT);

		if (!confReader.is_loaded())
		{
			gLog.Warning("Unable to load the configuration file, using defaults");
		}

		s_validateOnLoad = confReader.GetBoolValue("EEF", "OnActorLoad", true);
		s_doRecalcWeight = confReader.GetBoolValue("EEF", "RecalcPlayerInventoryWeightOnLoad", false);
		bool redirectDispelWornItemEnchantsVisitor = confReader.GetBoolValue("EEF", "RedirectDispelWornItemEnchantsVisitor", true);
		bool equipManagerHook = confReader.GetBoolValue("EEF", "ScriptEquipEventFix", false);

		auto& branchTrampoline = ISKSE::GetBranchTrampoline();

		if (redirectDispelWornItemEnchantsVisitor)
		{
			if (!hook::call5(
					branchTrampoline,
					inv_DispelWornItemEnchantsVisitor_addr,
					std::uintptr_t(Inventory_DispelWornItemEnchantsVisitor_inv_Hook),
					inv_DispelWornItemEnchantsVisitor_o))
			{
				gLog.Error("DispelWornItemEnchantsVisitor (inventory) failed");
			}

			if (!hook::call5(
					branchTrampoline,
					addrem_DispelWornItemEnchantsVisitor_addr,
					std::uintptr_t(Inventory_DispelWornItemEnchantsVisitor_addrem_Hook),
					addrem_DispelWornItemEnchantsVisitor_o))
			{
				gLog.Error("DispelWornItemEnchantsVisitor (add/remove) failed");
			}

			if (!hook::call5(
					branchTrampoline,
					updateArmorAbility1_addr,
					uintptr_t(UpdateArmorAbility_Hook1),
					updateArmorAbility_o))
			{
				gLog.Error("UpdateArmorAbility hook failed");
			}

			gLog.Message("RedirectDispelWornItemEnchantsVisitor ON");
		}

		if (equipManagerHook)
		{
			struct Assembly : JITASM::JITASM
			{
				Assembly(std::uintptr_t a_targetAddr) :
					JITASM(ISKSE::GetLocalTrampoline())
				{
					Xbyak::Label retnLabel;

					auto size = IAL::IsAE() ? 0x6 : 0x5;

					db(reinterpret_cast<Xbyak::uint8*>(a_targetAddr), size);

					jmp(ptr[rip + retnLabel]);

					L(retnLabel);
					dq(a_targetAddr + size);
				}
			};

			gLog.Message("Patching EquipManager::EquipItem");

			Assembly code(EquipItem_a);
			EquipItem_o = code.get<decltype(EquipItem_o)>();

			if (IAL::IsAE())
			{
				branchTrampoline.Write6Branch(
					EquipItem_a,
					std::uintptr_t(EquipItem_Hook));
			}
			else
			{
				branchTrampoline.Write5Branch(
					EquipItem_a,
					std::uintptr_t(EquipItem_Hook));
			}
		}

		if (s_validateOnLoad)
			gLog.Message("OnActorLoad ON");

		auto& si = ISKSE::GetSingleton();

		if (!si.GetInterface<SKSEMessagingInterface>()->RegisterListener(
				si.GetPluginHandle(),
				"SKSE",
				MessageHandler))
		{
			gLog.FatalError("Couldn't add message listener");
		}

		return true;
	}
}