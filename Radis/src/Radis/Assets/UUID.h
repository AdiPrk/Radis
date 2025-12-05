#pragma once

namespace Dog {

	class UUID
	{
	public:
		UUID();
		UUID(uint64_t uuid);
		UUID(const UUID&) = default;
		UUID(const std::string& name);

		operator uint64_t() const { return m_UUID; }

		bool operator==(const UUID& other) const { return m_UUID == other.m_UUID; }
		bool operator!=(const UUID& other) const { return m_UUID != other.m_UUID; }

		bool operator==(uint64_t other) const { return m_UUID == other; }
		bool operator!=(uint64_t other) const { return m_UUID != other; }

        uint64_t get() const { return m_UUID; }

	private:
		uint64_t m_UUID;
	};

}

namespace std {
	template <typename T> struct hash;

	template<>
	struct hash<Dog::UUID>
	{
		std::size_t operator()(const Dog::UUID& uuid) const
		{
			return (uint64_t)uuid;
		}
	};

}

