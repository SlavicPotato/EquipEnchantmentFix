#pragma once

namespace EEF
{
    typedef void(__cdecl* removeActiveEffect_t)(MagicTarget* target, ActiveEffect* effect, uint8_t unk0);

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
        stl::unordered_set<UInt32> m_data;
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
    bool Initialize();
}