// ddungeon.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <random>
#include <array>
#include <span>
#include <optional>
#include <algorithm>
#include <ranges>
#include <vector>
#include <map>
#include <string>
#include <format>
#include <charconv>
#include <tuple>

void clrscr()
{
    std::cout << "\033[2J\033[1;1H";
}

constexpr int monster_prob{ 30 };
constexpr int demon_prob{ 25 };
constexpr int demon_curse_prob{ 40 };
constexpr double demon_curse_perc{ 0.5 };
constexpr int gas_prob{ 25 };
constexpr int gas_poison_prob{ 40 };
constexpr double gas_poison_perc{ 0.5 };
constexpr int dropoff_prob{ 50 };
constexpr int slide_prob{ 50 };
constexpr int wand_backfire_prob{ 40 };
constexpr double wand_backfire_penalty{ 0.5 };
constexpr int treasure_prob{ 40 };
constexpr int tremor_prob{ 1 };
constexpr int room_per_level{ 4 };
constexpr int passages_per_room{ 4 };

enum class Feature : uint8_t { Monster = 1, Demon = 1 << 1, Gas = 1 << 2, Dropoff = 1 << 3, Slide = 1 << 4, Treasure = 1 << 5 };
enum class Action { Move, Fight, Wand, Dropoff, Exit, Trade, Overview };

bool test_feature(Feature feature, uint8_t features)
{
    return features &= std::to_underlying(feature);
}

class Random
{
public:
    static bool roll_probability(int probability) noexcept
    {
        return m_dist(m_mt) <= probability;
    }
    static int roll_max(int max) noexcept
    {
        std::uniform_int_distribution<> d{ 0, max };
        return d(m_mt);
    }
private:
    static inline std::mt19937 m_mt{ std::random_device{}() };
    static inline std::uniform_int_distribution<> m_dist{ 0,100 };
};

struct Room
{
    void stock(const int depth, const int roomnum) noexcept
    {
        m_roomnr = roomnum;
        visited = false;
        if (Random::roll_probability(monster_prob)) features |= std::to_underlying(Feature::Monster);
        if (Random::roll_probability(demon_prob)) features |= std::to_underlying(Feature::Demon);
        if (Random::roll_probability(gas_prob)) features |= std::to_underlying(Feature::Gas);
        if (Random::roll_probability(dropoff_prob)) features |= std::to_underlying(Feature::Dropoff);
        //if (Random::roll_probability(slide_prob)) features |= std::to_underlying(Feature::Slide);
        if (Random::roll_probability(treasure_prob)) features |= std::to_underlying(Feature::Treasure);
        auto x{ Random::roll_max(7) };
        auto y{ x * roomnum * depth };
        treasure = Random::roll_max(7) * roomnum * depth + 1;
        passages.fill(std::nullopt);
        monster_strength = Random::roll_max(7) * depth + roomnum + 1;
        monster_speed = Random::roll_max(7) * depth + roomnum + 1;
    }

    bool has_feature(Feature feature) const
    {
        return features & std::to_underlying(feature);
    }

    void remove_feature(Feature feature)
    {
        features &= ~std::to_underlying(feature);
    }
    void add_feature(Feature feature)
    {
        features |= std::to_underlying(feature);
    }

    int m_roomnr{ 0 };
    bool visited{ false };
    int treasure{ 0 };
    int num_passages{ 3 };  // 1..passages_per_room
    std::array<std::optional<int>, passages_per_room> passages;
    int slide{ 0 };
    int monster_strength{ 0 };
    int monster_speed{ 0 };
    std::uint8_t features{ 0 };
};

struct Level
{
    void init() noexcept
    {
        for (int roomnum{ 0 }; auto& room:rooms)
        {
            room.stock(m_depth, roomnum);
            ++roomnum;
        }
        rooms[0].features &= std::to_underlying(Feature::Slide);
        rooms[0].num_passages = 3;
    }
    std::array<Room, room_per_level> rooms;
    int m_depth{ 1 };
};

struct Player
{
    int depth{ 1 };
    int roomnr{ 0 };
    int strength{ 100 };
    int speed{ 100 };
    int gold{ 0 };
    int xp{ 0 };

    void get_cursed() noexcept
    {
        if (Random::roll_probability(demon_curse_prob))
        {
            std::cout << "You were cursed by a demon\n";
            speed *= demon_curse_perc;
        }
    }

    void get_gassed() noexcept
    {
        if (Random::roll_probability(gas_poison_prob))
        {
            std::cout << "you were gassed\n";
            strength *= gas_poison_perc;
        }
    }

    void wand(Room& room) noexcept
    {
        if (Random::roll_probability(wand_backfire_prob))
        {
            std::cout << "the wand backfires\n";
            speed *= wand_backfire_penalty;
        }
        else
        {
            std::cout << "the wand works\n";
            room.remove_feature(Feature::Monster);
            room.remove_feature(Feature::Demon);
            room.remove_feature(Feature::Gas);
        }
    }
};

/*
* Stimulating Simulations has a weird algorithm for creating passages that I don't really understand to be honest.
* It also looks at the previous and next array index without bounds checking. I assume this version of BASIC has 
* some kind of default behaviour for array out-of-bounds indexing that the program relies on. Does it wrap around
* or something like that? Anyway, I use a different algorithm that (I think) is a bit more robust.
* 
* Also, with tremors he only re-randomises the passages themselves. The number of passages for each room and the 
* configuration of slides remain the same. I re-randomise those too.
* 
* In Stimulating Simulations, line 150 states that with a 1% probability, the first 20 positions in array L (the passages) 
* is re-randomised.
* But at line 160, with an independent 1% probability, the first 20 positions of the array are set to zero (no passage).
* Is this a bug? Anyway, I just re-run create_passages().
*/

void create_passages(std::span<Room> rooms) noexcept
{
    for (int roomnr = 0; roomnr < rooms.size(); ++roomnr)
    {
        auto& room = rooms[roomnr];
        if (roomnr > 0)
        {
            room.num_passages = Random::roll_max(passages_per_room - 1) + 1;
        }
        room.remove_feature(Feature::Slide);
        if (Random::roll_probability(slide_prob)) room.add_feature(Feature::Slide);
        room.slide = Random::roll_max(room_per_level - 1);
        for (int i = 0; i < room.num_passages; ++i)
        {
            auto& passage = room.passages[i];
            if (passage) continue;
            int maxtries{ 10 };
            bool success{ false };
            for (int tries{ 0 }; !success && tries < maxtries; ++tries)
            {
                int target = Random::roll_max(room_per_level - 1);
                if (std::ranges::find(room.passages, target) != std::end(room.passages)) continue;
                if (target == roomnr) continue;
                auto start = std::begin(rooms[target].passages);
                auto last = start;
                std::advance(last, rooms[target].num_passages);
                auto itr = std::find_if(start, last, [](std::optional<int>& pass) {return pass == std::nullopt; });
                if (itr != last)
                {
                    passage = target;
                    *itr = roomnr;
                    success = true;
                }
            }
            room.slide = Random::roll_max(room_per_level - 1);
        }
    }
}

void tremor(std::span<Room> rooms) noexcept
{
    if (Random::roll_probability(tremor_prob))
    {
        create_passages(rooms);
    }
}

void print_room(const Room& room) noexcept
{
    if (room.has_feature(Feature::Demon))
    {
        std::cout << "There is a demon here \n";
    }
    if (room.has_feature(Feature::Monster))
    {
        std::cout << "There is a monster here \n";
        std::cout << "Monster speed is " << room.monster_speed << "\n";
        std::cout << "Monster strength is " << room.monster_strength << "\n";
    }
    if (room.has_feature(Feature::Gas)) std::cout << "There is gas here \n";
    if (room.has_feature(Feature::Dropoff)) std::cout << "There is a dropoff here \n";
    if (room.has_feature(Feature::Treasure))
    {
        std::cout << std::format("There are {} gold pieces here \n", room.treasure);
    }
    if (room.has_feature(Feature::Slide))
        std::cout << std::format("Slide to {}\n", room.slide);
    std::cout << std::format("There are passages from {} to ", room.m_roomnr);
    for (auto pass : room.passages)
    {
        if (pass)
            std::cout << *pass << " ";
    }
    std::cout << "\n";
}

void print_status(const Player& p, const Room& room, int depth) noexcept
{
    std::cout << "\n------------\n";
    std::cout << std::format("Gold {} Exp {}, Depth {}, Roomnr {}\n", p.gold, p.xp, depth, p.roomnr);
    std::cout << std::format("Your speed {}, strength {}\n\n", p.speed, p.strength);
    print_room(room);
    if (room.has_feature(Feature::Monster))
    {
        std::cout << "use f to fight\n";
    }
    if (room.has_feature(Feature::Dropoff))
    {
        std::cout << "use d to go down\n";
    }
    std::cout << "use w to use the magic wand\n";
    if (room.m_roomnr == 0)
    {
        std::cout << "use e to exit the dungeon\n";
        std::cout << "use t to trade experience\n";
    }
    std::cout << "use o to print a level overview\n";

    std::cout << "?\n\n";
}

void print_overview(const Level& l) noexcept
{
    std::cout << std::format("Overview of depth {}\n", l.m_depth);
    for (auto& room : l.rooms)
    {
        if (room.visited)
        {
            std::cout << std::format("Room {}\n", room.m_roomnr);
            print_room(room);
            std::cout << "\n\n";
        }
        else
        {
            std::cout << std::format("room {} has not been visited\n", room.m_roomnr);
        }
    }
}

std::tuple<Action, int> handle_input(Room& room, Player& player)
{
    std::string input;
    bool ok{ false };
    while (std::getline(std::cin, input))
    {
        if (input.size() > 2) continue;
        if (input == "f") // fight
            return std::make_tuple(Action::Fight, 0);
        if (input == "w") // wand
            return std::make_tuple(Action::Wand, 0);
        if (input == "d") // dropoff
            return std::make_tuple(Action::Dropoff, 0);
        if (input == "e" && room.m_roomnr == 0)
            return std::make_tuple(Action::Exit, 0);
        if (room.m_roomnr == 0 && input == "e")
            return std::make_tuple(Action::Exit, 0);
        if (input == "o")
            return std::make_tuple(Action::Overview, 0);
        if (input == "t")
            return std::make_tuple(Action::Trade, 0);
        int res;
        auto [p, ec] = std::from_chars(input.data(), input.data() + input.size(), res);
        if (ec != std::errc()) continue;
        if (room.has_feature(Feature::Slide) && res == room.slide)
        {
            std::cout << std::format("slide to {} \n", res);
            return std::make_tuple(Action::Move, res);
        }
        auto itr = std::ranges::find(room.passages, res);
        if (itr != std::end(room.passages))
        {
            std::cout << std::format("move to {} \n", res);
            return std::make_tuple(Action::Move, res);
        }
    }
}

void move(Player& p, int target) noexcept
{
    p.speed -= p.depth;
    p.strength -= p.depth;
    p.roomnr = target;
}

bool handle_hazards(Level& level, Player& p) noexcept
{
    tremor(level.rooms);
    auto room = level.rooms[p.roomnr];
    if (room.has_feature(Feature::Demon))
        p.get_cursed();
    if (room.has_feature(Feature::Gas))
        p.get_gassed();
    if (room.has_feature(Feature::Monster))
    {
        auto& room = level.rooms[p.roomnr];
        if (Random::roll_max(100) * p.speed > Random::roll_max(100) * room.monster_speed)
        {
            std::cout << "You escaped from the monster\n";
        }
        else
        {
            std::cout << "The monster hit you\n";
            p.strength -= room.monster_strength * 0.2;
        }
        return false;
    }
    return true;
}

void take_treasure(Level& level, Player& p) noexcept
{
    auto room = level.rooms[p.roomnr];
    if (room.has_feature(Feature::Treasure))
    {
        auto perc = Random::roll_max(100);
        auto found_gold = room.treasure* perc / 100;
        std::cout << std::format("you found {} pieces of gold\n", found_gold);
        p.gold += found_gold;
        p.xp += found_gold;
        room.remove_feature(Feature::Treasure);
    }
}

void fight_monster(Room& room, Player& p)
{
    if (!room.has_feature(Feature::Monster)) return;
    auto player_damage = p.strength * Random::roll_max(100) / 100;
    auto monster_damage = room.monster_strength * Random::roll_max(100) / 100;
    if (player_damage > room.monster_strength)
        player_damage = room.monster_strength;
    if (monster_damage > p.strength)
        monster_damage = p.strength;
    if (Random::roll_max(100) * room.monster_speed > Random::roll_max(100) * p.speed)
    {
        std::cout << "Monster attacks\n";
        p.strength -= monster_damage;
        room.monster_strength -= 0.5 * player_damage;
    }
    else
    {
        std::cout << "You attack\n";
        room.monster_strength -= player_damage;
        p.strength -= 0.5 * monster_damage;
        if (room.monster_strength <= 0)
        {
            std::cout << "You killed the monster\n";
            p.xp += 2 * player_damage;
            room.remove_feature(Feature::Monster);
        }
        else
        {
            std::cout << "The monster is still alive\n";
        }
    }
}

void exit_message(const Player& p) noexcept
{
    std::cout << "Congratulations, you escaped from the dungeon\n";
    std::cout << std::format("You have {} gold and {} experience\n", p.gold, p.xp);
    std::cout << std::format("Your strenght is {} and your speed is {}\n", p.strength, p.speed);
}

void trade_experience(Player& p) noexcept
{
    std::cout << std::format("Trading experience. Your experience = {}\n", p.xp);
    std::cout << "How much strenght do you want to buy?\n";
    std::string input;
    int res;
    while (std::getline(std::cin, input))
    {
        auto [ptr, ec] = std::from_chars(input.data(), input.data() + input.size(), res);
        if (ec != std::errc()) continue;
        if (res > p.xp) continue;
        p.xp -= res;
        p.strength += res;
        break;
    }
    std::cout << "How much speed do you want to buy?\n";
    while (std::getline(std::cin, input))
    {
        auto [ptr, ec] = std::from_chars(input.data(), input.data() + input.size(), res);
        if (ec != std::errc()) continue;
        if (res > p.xp) continue;
        p.xp -= res;
        p.speed += res;
        break;
    }
}

void dropoff(Level& level) noexcept
{
    level.m_depth++;
    level.init();
    create_passages(level.rooms);
}

int main()
{
    Level l{};
    l.init();
    create_passages(l.rooms);
    Player p{};
    bool done{ false };
    while(!done)
    {
        if (p.speed <= 0 || p.strength <= 0)
        {
            std::cout << "You died\n";
            done = true;
        }
        auto& room = l.rooms[p.roomnr];
        room.visited = true;
        print_status(p, room, l.m_depth);
        auto [action, target] = handle_input(room, p);
        switch (action)
        {
        case Action::Move:
            if (handle_hazards(l, p))  // do not get treasure when escaping from a monster
                take_treasure(l, p);
            move(p, target);
            break;
        case Action::Wand:
            p.wand(room);
            break;
        case Action::Fight:
            fight_monster(room, p);
            break;
        case Action::Dropoff:
            dropoff(l);
            break;
        case Action::Exit:
            done = true;
            exit_message(p);
            break;
        case Action::Overview:
            print_overview(l);
            break;
        case Action::Trade:
            trade_experience(p);
            break;
        }
    }
}
