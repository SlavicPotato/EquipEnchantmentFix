#pragma once

namespace EEF
{
	typedef void(__cdecl* removeActiveEffect_t)(MagicTarget* target, ActiveEffect* effect, uint8_t unk0);
	typedef void(__cdecl* updateArmorAbility_t)(Actor* a_actor, TESForm* a_form, BaseExtraList* extraData);
	typedef void(__cdecl* inv_DispelWornItemEnchantsVisitor_t)(Actor* a_actor);

	class EnchantmentEnforcerTask :
		public TaskDelegate
	{
	public:
		EnchantmentEnforcerTask() = default;

		virtual void Run() override;
		virtual void Dispose() override{};

		static void ProcessActor(Actor* a_actor);

		inline bool Add(Game::ObjectRefHandle a_handle)
		{
			IScopedLock lock(m_lock);
			return m_data.emplace(a_handle).second;
		}

		WCriticalSection m_lock;
		std::unordered_set<Game::ObjectRefHandle> m_data;
	};

	class PlayerInvWeightRecalcTask :
		public TaskDelegate
	{
	public:
		virtual void Run() override;
		virtual void Dispose() override{};
	};

	class EEFEventHandler :
		public BSTEventSink<TESEquipEvent>,
		public BSTEventSink<TESObjectLoadedEvent>,
		public BSTEventSink<TESInitScriptEvent>

	{
	protected:
		virtual EventResult ReceiveEvent(TESEquipEvent* evn, EventDispatcher<TESEquipEvent>* dispatcher) override;
		virtual EventResult ReceiveEvent(TESObjectLoadedEvent* evn, EventDispatcher<TESObjectLoadedEvent>* dispatcher) override;
		virtual EventResult ReceiveEvent(TESInitScriptEvent* evn, EventDispatcher<TESInitScriptEvent>* dispatcher) override;

	public:
		static EEFEventHandler* GetSingleton()
		{
			static EEFEventHandler handler;
			return &handler;
		}

	private:
		SKMP_FORCEINLINE void HandleEvent(TESEquipEvent* a_evn);
	};

	struct ItemEntry
	{
		ItemEntry(
			TESForm* a_form,
			BaseExtraList* a_extraList,
			EnchantmentItem* a_enchantment) :
			m_form(a_form),
			m_extraList(a_extraList),
			m_enchantment(a_enchantment)
		{}

		TESForm* m_form;
		BaseExtraList* m_extraList;
		EnchantmentItem* m_enchantment;
	};

	struct EquippedEnchantedArmorItemCollector
	{
		bool Accept(InventoryEntryData* a_entryData);

		std::vector<ItemEntry> m_results;
	};

	class MatchForm :
		public FormMatcher
	{
	public:
		MatchForm(TESForm* a_form) :
			m_form(a_form)
		{
		}

		virtual bool Matches(TESForm* a_form) const
		{
			return m_form == a_form;
		}

	private:
		TESForm* m_form;
	};

	struct FindItemResult
	{
		bool m_match{ false };
		bool m_equipped{ false };
		TESForm* m_form{ nullptr };
		BaseExtraList* m_extraData{ nullptr };
	};

	struct FindEquippedArmorItemVisitor  // :
		//ItemCounterBase
	{
		FindEquippedArmorItemVisitor(TESForm* a_match) :
			m_match(a_match)
		{
		}

		SKMP_FORCEINLINE bool Accept(InventoryEntryData* a_entryData);

		TESForm* m_match;
		FindItemResult m_result;
	};

	struct FindEquippedItemVisitor
	{
		FindEquippedItemVisitor(TESForm* a_match) :
			m_match(a_match)
		{
		}

		SKMP_FORCEINLINE bool Accept(InventoryEntryData* a_entryData);

		TESForm* m_match;
		FindItemResult m_result;
	};

	bool Initialize();
}