#pragma once

namespace EEF
{
    struct enchantmentEffectID_t
    {
        Game::FormID sourceItem;
        Game::FormID item;
        Game::FormID mgef;
    };

    static_assert(sizeof(enchantmentEffectID_t) == 0xC);

    SKMP_FORCEINLINE bool operator==(const enchantmentEffectID_t& a_lhs, const enchantmentEffectID_t& a_rhs) {
        return a_lhs.sourceItem == a_rhs.sourceItem && a_lhs.item == a_rhs.item && a_lhs.mgef == a_rhs.mgef;
    }

    SKMP_FORCEINLINE bool operator!=(const enchantmentEffectID_t& a_lhs, const enchantmentEffectID_t& a_rhs) {
        return a_lhs.sourceItem != a_rhs.sourceItem || a_lhs.item != a_rhs.item || a_lhs.mgef != a_rhs.mgef;
    }
}

STD_SPECIALIZE_HASH(::EEF::enchantmentEffectID_t)

namespace EEF
{
    typedef void(__cdecl* removeActiveEffect_t)(MagicTarget* target, ActiveEffect* effect, uint8_t unk0);
    typedef void(__cdecl* updateArmorAbility_t)(Actor* a_actor, TESForm* a_form, BaseExtraList* extraData);
    typedef void(__cdecl* inv_DispelWornItemEnchantsVisitor_t)(Actor* a_actor);

    class EnchantmentEnforcerTask :
        public TaskDelegate
    {
        friend class DMisc;

    public:
        EnchantmentEnforcerTask() = default;

        virtual void Run() override;
        virtual void Dispose() override {};

        SKMP_FORCEINLINE static void ProcessActor(Actor* a_actor);

        SKMP_FORCEINLINE bool Add(UInt32 a_formid)
        {
            IScopedCriticalSection _(&m_lock);
            return m_data.emplace(a_formid).second;
        }

        ICriticalSection m_lock;
        std::unordered_set<Game::FormID> m_data;
    };

    class PlayerInvWeightRecalcTask :
        public TaskDelegate
    {
    public:

        virtual void Run() override;
        virtual void Dispose() override {};

    };

    class EEFEventHandler :
        public BSTEventSink <TESEquipEvent>,
        public BSTEventSink <TESObjectLoadedEvent>,
        public BSTEventSink <TESInitScriptEvent>

    {
    protected:
        virtual EventResult	ReceiveEvent(TESEquipEvent* evn, EventDispatcher<TESEquipEvent>* dispatcher) override;
        virtual EventResult	ReceiveEvent(TESObjectLoadedEvent* evn, EventDispatcher<TESObjectLoadedEvent>* dispatcher) override;
        virtual EventResult	ReceiveEvent(TESInitScriptEvent* evn, EventDispatcher<TESInitScriptEvent>* dispatcher) override;

    public:
        static EEFEventHandler* GetSingleton() {
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

    struct EquippedEnchantedItemCollector
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

        virtual bool Matches(TESForm* a_form) const {
            return m_form == a_form;
        }

    private:

        TESForm* m_form;
    };

    struct FindItemResult
    {
        FindItemResult() : m_match(false) {}

        bool m_match;
        TESForm* m_form;
        BaseExtraList* m_extraData;
    };

    struct FindItemVisitor
    {
        FindItemVisitor(TESForm* a_match) : m_match(a_match) {}

        bool Accept(InventoryEntryData* a_entryData);

        TESForm* m_match;
        FindItemResult m_result;
    };

    bool Initialize();
}