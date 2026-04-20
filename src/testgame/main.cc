#include <charconv>
#include <iostream>
#include <print>
#include <string>

class HealthGame final {
public:
    void run() {
        std::println("=== Health Game ===");
        std::println("Enter a number to deal damage. Health at 0 or below resets.");

        while (true) {
            std::println("\nHealth: {}", static_cast<int>(m_health));
            std::print("Damage: ");

            std::string input{};
            if (!std::getline(std::cin, input)) {
                break;
            }

            int damage{};
            auto [ptr, ec] = std::from_chars(input.data(), input.data() + input.size(), damage);
            if (ec != std::errc{}) {
                std::println("Invalid input.");
                continue;
            }

            apply_damage(damage);
        }
    }

private:
    int m_health{100};

    void apply_damage(const int damage) noexcept {
        m_health -= damage;
        if (m_health <= 0) {
            std::println("ur dead");
            m_health = 100;
        }
    }
};

int main() {
    HealthGame{}.run();
}
