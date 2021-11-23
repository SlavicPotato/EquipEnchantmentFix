#pragma once

#include <ext/ISKSE.h>

class ISKSE :
	public ISKSEBase<
		SKSEInterfaceFlags::kMessaging |
			SKSEInterfaceFlags::kTask |
			SKSEInterfaceFlags::kTrampoline,
		64ui64,
		32ui64>
{
public:
	[[nodiscard]] SKMP_FORCEINLINE static auto& GetSingleton()
	{
		return m_Instance;
	}

	[[nodiscard]] SKMP_FORCEINLINE static auto& GetBranchTrampoline()
	{
		return m_Instance.GetTrampoline(TrampolineID::kBranch);
	}

	[[nodiscard]] SKMP_FORCEINLINE static auto& GetLocalTrampoline()
	{
		return m_Instance.GetTrampoline(TrampolineID::kLocal);
	}

private:
	ISKSE() = default;

	virtual void OnLogOpen() override;
	virtual const char* GetLogPath() const override;
	virtual const char* GetPluginName() const override;
	virtual std::uint32_t GetPluginVersion() const override;
	virtual bool CheckRuntimeVersion(std::uint32_t a_version) const;

	static ISKSE m_Instance;
};