#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <algorithm>
#include <chrono>
#include <clocale>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <optional>
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <cstdint>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
    std::filesystem::path findDataRoot()
    {
        const std::filesystem::path candidates[] = {
            std::filesystem::current_path(),
            std::filesystem::current_path().parent_path(),
            std::filesystem::current_path().parent_path().parent_path()};

        for (const auto &base : candidates)
        {
            if (base.empty())
                continue;
            const auto dataDir = base / "data";
            if (std::filesystem::exists(dataDir) && std::filesystem::is_directory(dataDir))
                return dataDir;
        }

        return {};
    }

    bool loadTextureFromCandidates(sf::Texture &texture, const std::vector<std::filesystem::path> &candidates)
    {
        for (const auto &candidate : candidates)
        {
            if (std::filesystem::exists(candidate) && texture.loadFromFile(candidate.string()))
                return true;
        }
        return false;
    }

    std::vector<std::filesystem::path> makeCandidatePaths(const std::filesystem::path &relativePath)
    {
        return {
            relativePath,
            std::filesystem::path("../") / relativePath,
            std::filesystem::path("../../") / relativePath,
        };
    }

    bool loadSystemUIFont(sf::Font &font)
    {
        return font.openFromFile("C:/Windows/Fonts/msyh.ttc") ||
               font.openFromFile("C:/Windows/Fonts/simhei.ttf") ||
               font.openFromFile("C:/Windows/Fonts/arial.ttf");
    }

    // 逻辑分辨率固定为 1280x720
    const float LOGIC_WIDTH = 1280.f;
    const float LOGIC_HEIGHT = 720.f;

    // 全局记录上一次窗口的物理尺寸和最大化状态
    sf::Vector2u g_lastWindowSize(1280, 720);
    bool g_wasMaximized = false;

#ifdef _WIN32
    void maximizeWindow(sf::RenderWindow &window)
    {
        HWND hwnd = reinterpret_cast<HWND>(window.getNativeHandle());
        ShowWindow(hwnd, SW_MAXIMIZE);
    }

    bool isWindowMaximized(sf::RenderWindow &window)
    {
        HWND hwnd = reinterpret_cast<HWND>(window.getNativeHandle());
        WINDOWPLACEMENT placement;
        GetWindowPlacement(hwnd, &placement);
        return placement.showCmd == SW_SHOWMAXIMIZED;
    }
#else
    void maximizeWindow(sf::RenderWindow &) {}
    bool isWindowMaximized(sf::RenderWindow &) { return false; }
#endif
}

// ---------- 卡牌定义 ----------
enum class CardType
{
    ExplodingKitten,
    Defuse,
    SeeTheFuture,
    Prophecy,
    Shuffle,
    DrawFromBottom,
    Skip,
    Reverse,
    Attack,
    Favor,
    Exchange
};

std::string cardName(CardType type)
{
    switch (type)
    {
    case CardType::ExplodingKitten:
        return u8"\u70b8\u5f39";
    case CardType::Defuse:
        return u8"\u62c6\u9664";
    case CardType::SeeTheFuture:
        return u8"\u900f\u89c6";
    case CardType::Prophecy:
        return u8"\u9884\u8a00";
    case CardType::Shuffle:
        return u8"\u6d17\u724c";
    case CardType::DrawFromBottom:
        return u8"\u62bd\u5e95";
    case CardType::Skip:
        return u8"\u8df3\u8fc7";
    case CardType::Reverse:
        return u8"\u8f6c\u5411";
    case CardType::Attack:
        return u8"\u7529\u9505";
    case CardType::Favor:
        return u8"\u7d22\u8981";
    case CardType::Exchange:
        return u8"\u4ea4\u6362";
    default:
        return u8"\u672a\u77e5";
    }
}

sf::Color cardColor(CardType type)
{
    switch (type)
    {
    case CardType::ExplodingKitten:
        return sf::Color::Red;
    case CardType::Defuse:
        return sf::Color::Green;
    case CardType::Exchange:
        return sf::Color(148, 0, 211);
    case CardType::Prophecy:
        return sf::Color(255, 140, 0);
    default:
        return sf::Color::Blue;
    }
}

sf::String makeSfUtf8String(const std::string &utf8)
{
    return sf::String::fromUtf8(utf8.begin(), utf8.end());
}

std::string cardTextureFilename(CardType type)
{
    switch (type)
    {
    case CardType::Defuse:
        return "chaichu.png";
    case CardType::SeeTheFuture:
        return "toushi.png";
    case CardType::Prophecy:
        return "yuyan.png";
    case CardType::Shuffle:
        return "xipai.png";
    case CardType::DrawFromBottom:
        return "choudi.png";
    case CardType::Skip:
        return "tiaoguo.png";
    case CardType::Reverse:
        return "zhuanxiang.png";
    case CardType::Attack:
        return "shuaiguo.png";
    case CardType::Favor:
        return "suoyao.png";
    case CardType::Exchange:
        return "jiaohuan.png";
    default:
        return {};
    }
}

struct Card
{
    CardType type;
    explicit Card(CardType t) : type(t) {}
};

// ---------- 牌库 ----------
class Deck
{
    std::vector<Card> cards;
    std::mt19937 rng;

public:
    Deck() : rng(std::chrono::steady_clock::now().time_since_epoch().count()) {}

    void addCard(const Card &card) { cards.push_back(card); }

    void insertCardAt(const Card &card, int position)
    {
        if (position < 0 || position > static_cast<int>(cards.size()))
            position = static_cast<int>(cards.size());
        cards.insert(cards.begin() + position, card);
    }

    void shuffle()
    {
        std::shuffle(cards.begin(), cards.end(), rng);
    }

    Card drawTop()
    {
        Card c = cards.back();
        cards.pop_back();
        return c;
    }

    Card drawBottom()
    {
        Card c = cards.front();
        cards.erase(cards.begin());
        return c;
    }

    bool isEmpty() const { return cards.empty(); }
    size_t size() const { return cards.size(); }
    const std::vector<Card> &getCards() const { return cards; }
};

// ---------- 玩家 ----------
class Player
{
public:
    std::string name;
    std::vector<Card> hand;
    bool alive = true;
    bool isAI = false;

    explicit Player(std::string n) : name(std::move(n)) {}

    void drawCards(Deck &deck, int count)
    {
        for (int i = 0; i < count; ++i)
            if (!deck.isEmpty())
                hand.push_back(deck.drawTop());
    }

    bool hasCard(CardType type) const
    {
        for (const auto &c : hand)
            if (c.type == type)
                return true;
        return false;
    }

    int findCardIndex(CardType type) const
    {
        for (size_t i = 0; i < hand.size(); ++i)
            if (hand[i].type == type)
                return static_cast<int>(i);
        return -1;
    }

    void removeCard(int index)
    {
        if (index >= 0 && index < static_cast<int>(hand.size()))
            hand.erase(hand.begin() + index);
    }
};

// ---------- 游戏核心 ----------
class Game
{
public:
    enum class PendingAction
    {
        None,
        Attack,
        Favor,
        Exchange
    };

    Deck deck;
    std::vector<Player> players;
    std::vector<int> bonusTurns;
    int currentPlayer = 0;
    bool direction = true;
    bool mustDrawFromBottom = false;
    bool gameOver = false;
    bool drawRequired = true;
    int forcedNextPlayer = -1;
    int forcedTargetIndex = -1;
    int returnToPlayer = -1;
    bool returnPending = false;
    int attackOriginIndex = -1;
    int attackOriginStep = 1;
    int attackChainSource = -1;
    bool waitingForAttackResponse = false;
    PendingAction pendingAction = PendingAction::None;
    std::string message;
    bool drawPaused = false;
    float drawPauseSeconds = 0.f;
    sf::Clock drawPauseClock;
    sf::Clock turnClock;
    const float turnTimeLimitSeconds = 30.f;

    Game(int numPlayers)
        : bonusTurns(numPlayers, 0)
    {
        for (int i = 1; i <= numPlayers; ++i)
            players.emplace_back(std::string(u8"\u73a9\u5bb6") + std::to_string(i));

        if (!players.empty())
            players[0].isAI = false;
        for (size_t i = 1; i < players.size(); ++i)
            players[i].isAI = true;

        currentPlayer = 0;
        setupDeck(numPlayers);
        message = std::string(u8"\u8f6e\u5230 ") + players[currentPlayer].name;
        turnClock.restart();
    }

    void setupDeck(int numPlayers)
    {
        int totalBombs = numPlayers - 1;
        int totalDefuses = 6;
        int otherCards = 53 - totalBombs - totalDefuses;
        std::vector<CardType> funcTypes = {
            CardType::SeeTheFuture, CardType::Prophecy, CardType::Shuffle,
            CardType::DrawFromBottom, CardType::Skip, CardType::Reverse,
            CardType::Attack, CardType::Favor, CardType::Exchange};

        for (int i = 0; i < otherCards; ++i)
            deck.addCard(Card(funcTypes[i % funcTypes.size()]));
        deck.shuffle();

        for (auto &p : players)
            for (int i = 0; i < 4; ++i)
                p.hand.push_back(deck.drawTop());

        for (auto &p : players)
            p.hand.emplace_back(CardType::Defuse);

        for (int i = 0; i < totalBombs; ++i)
            deck.addCard(Card(CardType::ExplodingKitten));

        int remainingDefuse = totalDefuses - numPlayers;
        for (int i = 0; i < remainingDefuse; ++i)
            deck.addCard(Card(CardType::Defuse));

        deck.shuffle();
    }

    Player *getCurrentPlayer() { return &players[currentPlayer]; }

    bool isGameOver()
    {
        int alive = 0;
        for (const auto &p : players)
            if (p.alive)
                ++alive;
        return alive <= 1;
    }

    Player *getWinner()
    {
        for (auto &p : players)
            if (p.alive)
                return &p;
        return nullptr;
    }

    void advanceToNextAlivePlayer()
    {
        if (gameOver)
            return;

        int step = direction ? 1 : -1;
        int next = currentPlayer;
        do
            next = (next + step + static_cast<int>(players.size())) % static_cast<int>(players.size());
        while (!players[next].alive && next != currentPlayer);

        currentPlayer = next;
        if (isGameOver())
            gameOver = true;
    }

    int findNextAliveFrom(int idx, int step) const
    {
        int n = static_cast<int>(players.size());
        int next = idx;
        do
            next = (next + step + n) % n;
        while (!players[next].alive && next != idx);
        return next;
    }

    void nextTurn()
    {
        if (gameOver)
            return;

        if (returnPending && forcedTargetIndex >= 0 && currentPlayer == forcedTargetIndex)
        {
            int candidate = -1;
            if (attackOriginIndex >= 0)
            {
                candidate = findNextAliveFrom(attackOriginIndex, attackOriginStep);
            }

            if (candidate < 0 || !players[candidate].alive)
            {
                int step = direction ? 1 : -1;
                candidate = findNextAliveFrom(currentPlayer, step);
            }

            currentPlayer = candidate;
            returnPending = false;
            forcedNextPlayer = -1;
            forcedTargetIndex = -1;
            returnToPlayer = -1;
            attackOriginIndex = -1;
            attackOriginStep = 1;
            attackChainSource = -1;

            drawRequired = true;
            turnClock.restart();
            message = std::string(u8"\u8f6e\u5230 ") + players[currentPlayer].name;
            return;
        }

        mustDrawFromBottom = false;
        pendingAction = PendingAction::None;

        if (players[currentPlayer].alive && bonusTurns[currentPlayer] > 0)
        {
            --bonusTurns[currentPlayer];
            drawRequired = true;
            turnClock.restart();
            message = std::string(u8"\u8f6e\u5230 ") + players[currentPlayer].name + std::string(u8"\uff08\u989d\u5916\u56de\u5408\uff09");
            return;
        }

        if (forcedNextPlayer >= 0 && forcedNextPlayer < static_cast<int>(players.size()) && players[forcedNextPlayer].alive)
        {
            currentPlayer = forcedNextPlayer;
            forcedNextPlayer = -1;
        }
        else
        {
            forcedNextPlayer = -1;
            advanceToNextAlivePlayer();
        }

        if (!gameOver)
        {
            drawRequired = true;
            turnClock.restart();
            message = std::string(u8"\u8f6e\u5230 ") + players[currentPlayer].name;
        }
    }

    bool isWaitingForTarget() const
    {
        return pendingAction != PendingAction::None;
    }

    bool canDrawNow() const
    {
        return !gameOver && !drawPaused && drawRequired && !isWaitingForTarget() && players[currentPlayer].alive;
    }

    int getRemainingDrawSeconds() const
    {
        if (!canDrawNow() && !waitingForAttackResponse)
            return -1;

        float remaining = turnTimeLimitSeconds - turnClock.getElapsedTime().asSeconds();
        if (remaining <= 0.f)
            return 0;
        return static_cast<int>(remaining + 0.999f);
    }

    std::vector<int> getSelectableTargets() const
    {
        std::vector<int> targets;
        if (!isWaitingForTarget())
            return targets;

        for (int i = 0; i < static_cast<int>(players.size()); ++i)
        {
            if (!players[i].alive)
                continue;

            if (pendingAction == PendingAction::Attack)
            {
                targets.push_back(i);
                continue;
            }

            if (i != currentPlayer)
                targets.push_back(i);
        }
        return targets;
    }

    int chooseAIAttackTarget(int aiIndex)
    {
        for (int i = 0; i < static_cast<int>(players.size()); ++i)
        {
            if (i == aiIndex)
                continue;
            if (!players[i].alive)
                continue;
            if (!players[i].isAI)
                return i;
        }
        std::vector<int> candidates;
        for (int i = 0; i < static_cast<int>(players.size()); ++i)
        {
            if (i == aiIndex)
                continue;
            if (!players[i].alive)
                continue;
            candidates.push_back(i);
        }
        if (candidates.empty())
            return -1;
        std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        return candidates[std::uniform_int_distribution<int>(0, static_cast<int>(candidates.size()) - 1)(rng)];
    }

    int chooseAITargetForStealOrExchange(int aiIndex)
    {
        std::vector<int> bestTargets;
        int bestCount = -1;
        for (int i = 0; i < static_cast<int>(players.size()); ++i)
        {
            if (i == aiIndex)
                continue;
            if (!players[i].alive)
                continue;
            int cnt = static_cast<int>(players[i].hand.size());
            if (cnt > bestCount)
            {
                bestCount = cnt;
                bestTargets.clear();
                bestTargets.push_back(i);
            }
            else if (cnt == bestCount)
            {
                bestTargets.push_back(i);
            }
        }
        if (bestTargets.empty())
            return -1;
        std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
        return bestTargets[std::uniform_int_distribution<int>(0, static_cast<int>(bestTargets.size()) - 1)(rng)];
    }

    bool canAIUseAttackNow(int aiIndex) const
    {
        return attackChainSource < 0 || attackChainSource != aiIndex;
    }

    int findAIPlayableCardIndex(Player &ai, bool attackResponseMode)
    {
        const std::vector<CardType> priority = attackResponseMode
                                                   ? std::vector<CardType>{CardType::Attack, CardType::Skip, CardType::Reverse}
                                                   : std::vector<CardType>{CardType::Attack, CardType::Favor, CardType::Exchange, CardType::DrawFromBottom, CardType::Skip, CardType::Reverse, CardType::Prophecy, CardType::SeeTheFuture, CardType::Shuffle};

        for (CardType type : priority)
        {
            if (type == CardType::Attack && !canAIUseAttackNow(currentPlayer))
                continue;

            int index = ai.findCardIndex(type);
            if (index >= 0)
                return index;
        }

        return -1;
    }

    void clearAttackResponseChain()
    {
        waitingForAttackResponse = false;
        returnPending = false;
        forcedNextPlayer = -1;
        forcedTargetIndex = -1;
        attackOriginIndex = -1;
        attackOriginStep = 1;
        attackChainSource = -1;
        returnToPlayer = -1;
    }

    int aiPendingTarget = -1;
    bool aiWillSelectTarget = false;

    void selectTarget(int targetIndex)
    {
        if (!isWaitingForTarget())
            return;

        if (targetIndex < 0 || targetIndex >= static_cast<int>(players.size()) || !players[targetIndex].alive)
            return;

        Player &actor = *getCurrentPlayer();

        if (pendingAction == PendingAction::Attack)
        {
            pendingAction = PendingAction::None;
            if (targetIndex == currentPlayer)
            {
                message = actor.name + std::string(u8" \u5bf9\u81ea\u5df1\u7529\u9505\uff1a\u65e0\u989d\u5916\u56de\u5408");
            }
            else
            {
                int attacker = currentPlayer;
                int step = direction ? 1 : -1;

                if (attackOriginIndex < 0)
                {
                    attackOriginIndex = attacker;
                    attackOriginStep = step;
                }

                forcedTargetIndex = targetIndex;
                forcedNextPlayer = targetIndex;
                attackChainSource = attackOriginIndex;

                if (returnToPlayer < 0)
                {
                    int nextAfterOrigin = findNextAliveFrom(attackOriginIndex, attackOriginStep);
                    returnToPlayer = nextAfterOrigin;
                }

                waitingForAttackResponse = true;
                drawRequired = false;
                forcedNextPlayer = targetIndex;
                currentPlayer = targetIndex;
                message = players[attacker].name + std::string(u8" 甩锅给 ") + players[targetIndex].name + std::string(u8"，") + players[targetIndex].name + std::string(u8" 可以出牌反击或摸牌");
            }
            return;
        }

        if (targetIndex == currentPlayer)
            return;

        if (pendingAction == PendingAction::Favor)
        {
            pendingAction = PendingAction::None;
            Player &victim = players[targetIndex];
            if (!victim.hand.empty())
            {
                std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
                int idx = std::uniform_int_distribution<int>(0, static_cast<int>(victim.hand.size()) - 1)(rng);
                Card stolen = victim.hand[idx];
                victim.hand.erase(victim.hand.begin() + idx);
                actor.hand.push_back(stolen);
                message = actor.name + std::string(u8" \u4ece ") + victim.name + std::string(u8" \u7d22\u8981\u5230 ") + std::string(u8"\u201c") + cardName(stolen.type) + std::string(u8"\u201d");
            }
            else
            {
                message = victim.name + std::string(u8" \u624b\u724c\u4e3a\u7a7a\uff0c\u65e0\u6cd5\u7d22\u8981");
            }
            return;
        }

        if (pendingAction == PendingAction::Exchange)
        {
            pendingAction = PendingAction::None;
            Player &other = players[targetIndex];
            actor.hand.swap(other.hand);
            message = actor.name + std::string(u8" \u4e0e ") + other.name + std::string(u8" \u4ea4\u6362\u4e86\u6240\u6709\u624b\u724c");
        }
    }

    void update()
    {
        if (drawPaused)
        {
            if (drawPauseClock.getElapsedTime().asSeconds() >= drawPauseSeconds)
            {
                drawPaused = false;
                if (aiWillSelectTarget)
                {
                    aiWillSelectTarget = false;
                    int tgt = aiPendingTarget;
                    aiPendingTarget = -1;
                    selectTarget(tgt);
                    drawPaused = true;
                    drawPauseSeconds = 3.f;
                    drawPauseClock.restart();
                    return;
                }
                if (drawRequired)
                {
                    if (!gameOver && players[currentPlayer].isAI && players[currentPlayer].alive)
                    {
                        endPlayPhase();
                        drawPaused = true;
                        drawPauseSeconds = 5.f;
                        drawPauseClock.restart();
                        return;
                    }

                    if (!players[currentPlayer].alive)
                    {
                        nextTurn();
                        return;
                    }

                    return;
                }

                nextTurn();
            }
            return;
        }

        if (!gameOver && players[currentPlayer].isAI && players[currentPlayer].alive && !isWaitingForTarget())
        {
            if (waitingForAttackResponse && currentPlayer == forcedTargetIndex)
            {
                Player &ai = players[currentPlayer];
                int idxAttack = canAIUseAttackNow(currentPlayer) ? ai.findCardIndex(CardType::Attack) : -1;
                if (idxAttack >= 0)
                {
                    playCard(idxAttack);
                    message = ai.name + std::string(u8" 出了 ") + cardName(CardType::Attack);
                    int tgt = chooseAIAttackTarget(currentPlayer);
                    if (tgt >= 0)
                    {
                        aiPendingTarget = tgt;
                        aiWillSelectTarget = true;
                    }
                    drawPaused = true;
                    drawPauseSeconds = 3.f;
                    drawPauseClock.restart();
                    return;
                }

                int idxSkip = ai.findCardIndex(CardType::Skip);
                int idxRev = ai.findCardIndex(CardType::Reverse);
                if (idxSkip >= 0)
                {
                    playCard(idxSkip);
                    message = ai.name + std::string(u8" 出了 ") + cardName(CardType::Skip) + std::string(u8"，跳过摸牌");
                    drawPaused = true;
                    drawPauseSeconds = 3.f;
                    drawPauseClock.restart();
                    return;
                }
                if (idxRev >= 0)
                {
                    playCard(idxRev);
                    message = ai.name + std::string(u8" 出了 ") + cardName(CardType::Reverse) + std::string(u8"，方向反转");
                    drawPaused = true;
                    drawPauseSeconds = 3.f;
                    drawPauseClock.restart();
                    return;
                }

                endPlayPhase();
                drawPaused = true;
                drawPauseSeconds = 3.f;
                drawPauseClock.restart();
                return;
            }

            if (drawRequired)
            {
                Player &ai = players[currentPlayer];
                auto tryPlay = [&](CardType t) -> int
                { return ai.findCardIndex(t); };
                std::vector<CardType> priority = {CardType::Attack, CardType::Favor, CardType::Exchange, CardType::DrawFromBottom, CardType::Skip, CardType::Reverse, CardType::Prophecy, CardType::SeeTheFuture, CardType::Shuffle};
                for (auto ct : priority)
                {
                    if (ct == CardType::Attack && !canAIUseAttackNow(currentPlayer))
                        continue;

                    int idx = tryPlay(ct);
                    if (idx >= 0)
                    {
                        playCard(idx);
                        message = ai.name + std::string(u8" 出了 ") + cardName(ct);
                        if (ct == CardType::Favor || ct == CardType::Exchange || ct == CardType::Attack)
                        {
                            int target = -1;
                            if (ct == CardType::Favor || ct == CardType::Exchange)
                                target = chooseAITargetForStealOrExchange(currentPlayer);
                            else if (ct == CardType::Attack)
                                target = chooseAIAttackTarget(currentPlayer);
                            if (target >= 0)
                            {
                                aiPendingTarget = target;
                                aiWillSelectTarget = true;
                            }
                        }
                        drawPaused = true;
                        drawPauseSeconds = 3.f;
                        drawPauseClock.restart();
                        return;
                    }
                }

                endPlayPhase();
                drawPaused = true;
                drawPauseSeconds = 3.f;
                drawPauseClock.restart();
                return;
            }
        }

        if ((canDrawNow() || waitingForAttackResponse) && turnClock.getElapsedTime().asSeconds() >= turnTimeLimitSeconds)
        {
            if (waitingForAttackResponse)
            {
                message = getCurrentPlayer()->name + std::string(u8" 超时，系统强制摸牌");
                waitingForAttackResponse = false;
                returnPending = false;
                forcedTargetIndex = -1;
                attackOriginIndex = -1;
                attackOriginStep = 1;
                attackChainSource = -1;
                returnToPlayer = -1;
            }
            else
            {
                message = getCurrentPlayer()->name + std::string(u8" 超时，系统强制从牌顶抽牌");
            }
            mustDrawFromBottom = false;
            handleDrawnCard(deck.drawTop());
        }
    }

    void endPlayPhase()
    {
        if (!canDrawNow() && !waitingForAttackResponse)
            return;

        if (waitingForAttackResponse)
        {
            message = getCurrentPlayer()->name + std::string(u8" 选择摸牌，甩锅链结束");
            waitingForAttackResponse = false;
            returnPending = true;
        }

        Card c = mustDrawFromBottom ? deck.drawBottom() : deck.drawTop();
        mustDrawFromBottom = false;
        handleDrawnCard(c);
    }

    void handleDrawnCard(Card c)
    {
        if (gameOver)
            return;

        Player &player = *getCurrentPlayer();
        if (c.type == CardType::ExplodingKitten)
        {
            if (player.hasCard(CardType::Defuse))
            {
                int idx = player.findCardIndex(CardType::Defuse);
                player.removeCard(idx);
                std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
                int pos = std::uniform_int_distribution<int>(0, static_cast<int>(deck.size()))(rng);
                deck.insertCardAt(c, pos);
                message = player.name + std::string(u8" \u62bd\u5230\u70b8\u5f39\uff0c\u5df2\u4f7f\u7528\u62c6\u9664\u5e76\u79d8\u5bc6\u653e\u56de\u724c\u5e93\u3002");
                drawPaused = true;
                drawPauseSeconds = 5.f;
                drawPauseClock.restart();
                drawRequired = false;
            }
            else
            {
                player.alive = false;
                player.hand.clear();
                message = player.name + std::string(u8" \u62bd\u5230\u70b8\u5f39\uff0c\u51fa\u5c40\uff01");
                if (isGameOver())
                {
                    gameOver = true;
                    return;
                }
                else
                {
                    drawPaused = true;
                    drawPauseSeconds = 5.f;
                    drawPauseClock.restart();
                    return;
                }
            }
        }
        else
        {
            player.hand.push_back(c);
            message = player.name + std::string(u8" \u62bd\u5230\u4e86 ") + cardName(c.type);
        }

        drawPaused = true;
        drawPauseSeconds = 5.f;
        drawPauseClock.restart();
        drawRequired = false;
    }

    void playCard(int handIndex)
    {
        if (gameOver)
            return;

        if (drawPaused)
        {
            Player *current = getCurrentPlayer();
            if (current)
                message = current->name + std::string(u8" 刚抽完牌，当前处于停顿中，不能出牌");
            return;
        }

        Player &p = *getCurrentPlayer();
        if (handIndex < 0 || handIndex >= static_cast<int>(p.hand.size()))
            return;

        Card card = p.hand[handIndex];

        if (waitingForAttackResponse)
        {
            if (card.type != CardType::Attack && card.type != CardType::Skip && card.type != CardType::Reverse)
            {
                message = p.name + std::string(u8" 被甩锅了！只能出甩锅牌（继续传递）、跳过牌或转向牌来躲避摸牌");
                drawPaused = true;
                drawPauseSeconds = 5.f;
                drawPauseClock.restart();
                return;
            }
        }

        if (attackChainSource >= 0 && attackChainSource == currentPlayer && card.type == CardType::Attack)
        {
            message = std::string(u8"甩锅无效！你已被甩过一次，必须摸牌。系统已将甩锅牌归还。");
            return;
        }

        p.removeCard(handIndex);

        switch (card.type)
        {
        case CardType::SeeTheFuture:
        {
            std::string top3 = u8"\u724c\u5e93\u9876\u4e09\u5f20\uff1a";
            int count = 0;
            auto cards = deck.getCards();
            for (int i = static_cast<int>(cards.size()) - 1; i >= 0 && count < 3; --i, ++count)
                top3 += cardName(cards[i].type) + " ";
            message = top3;
            break;
        }
        case CardType::Prophecy:
        {
            auto cards = deck.getCards();
            int distanceFromTop = 1;
            int foundDistance = -1;
            for (int i = static_cast<int>(cards.size()) - 1; i >= 0; --i, ++distanceFromTop)
            {
                if (cards[i].type == CardType::ExplodingKitten)
                {
                    foundDistance = distanceFromTop;
                    break;
                }
            }

            if (foundDistance == -1)
                message = u8"\u9884\u8a00\uff1a\u76ee\u524d\u724c\u5e93\u4e2d\u6ca1\u6709\u70b8\u5f39\u724c";
            else
                message = std::string(u8"\u9884\u8a00\uff1a\u6700\u8fd1\u70b8\u5f39\u5728\u8ddd\u79bb\u724c\u9876\u7b2c ") + std::to_string(foundDistance) + std::string(u8" \u5f20");
            break;
        }
        case CardType::Shuffle:
            deck.shuffle();
            message = u8"\u724c\u5e93\u5df2\u6d17\u4e71";
            break;
        case CardType::DrawFromBottom:
            mustDrawFromBottom = true;
            message = u8"\u4e0b\u6b21\u62bd\u724c\u5c06\u4ece\u5e95\u90e8\u62bd\u53d6";
            break;
        case CardType::Skip:
            if (waitingForAttackResponse)
            {
                message = p.name + std::string(u8" 使用跳过牌躲避摸牌！");
                waitingForAttackResponse = false;
                returnPending = true;
            }
            else
            {
                message = p.name + std::string(u8" 使用跳过牌！");
            }
            drawRequired = false;
            nextTurn();
            break;
        case CardType::Reverse:
            direction = !direction;
            if (waitingForAttackResponse)
            {
                message = p.name + std::string(u8" 使用转向牌躲避摸牌！");
                clearAttackResponseChain();
            }
            else
            {
                message = p.name + std::string(u8" 使用转向牌反转方向！");
            }
            drawRequired = false;
            nextTurn();
            break;
        case CardType::Attack:
            if (waitingForAttackResponse)
            {
                pendingAction = PendingAction::Attack;
                message = p.name + std::string(u8" 反击甩锅！请选择甩锅目标");
            }
            else if (attackChainSource >= 0 && attackChainSource == currentPlayer)
            {
                p.hand.push_back(card);
                message = std::string(u8"甩锅无效！你已被甩过一次，必须摸牌。系统已将甩锅牌归还。");
            }
            else
            {
                pendingAction = PendingAction::Attack;
                message = u8"请选择甩锅目标（可选自己）";
            }
            break;
        case CardType::Favor:
            pendingAction = PendingAction::Favor;
            message = u8"\u8bf7\u9009\u62e9\u88ab\u7d22\u8981\u7684\u73a9\u5bb6";
            break;
        case CardType::Exchange:
            pendingAction = PendingAction::Exchange;
            message = u8"\u8bf7\u9009\u62e9\u4ea4\u6362\u624b\u724c\u7684\u73a9\u5bb6";
            break;
        default:
            break;
        }
    }
};

// ---------- 工具函数：处理窗口 resize 并维持固定逻辑分辨率 ----------
void applyResizeView(sf::RenderWindow &window, const sf::View &fixedView, unsigned int winWidth, unsigned int winHeight)
{
    float winW = static_cast<float>(winWidth);
    float winH = static_cast<float>(winHeight);
    float viewW = LOGIC_WIDTH;
    float viewH = LOGIC_HEIGHT;
    float viewAspect = viewW / viewH;
    float winAspect = winW / winH;

    sf::View newView(fixedView);
    if (winAspect > viewAspect)
    {
        // 窗口更宽，上下黑边
        float scale = winH / viewH;
        float newWidth = viewW * scale;
        float offsetX = (winW - newWidth) * 0.5f;
        newView.setViewport(sf::FloatRect(sf::Vector2f(offsetX / winW, 0.f), sf::Vector2f(newWidth / winW, 1.f)));
    }
    else
    {
        // 窗口更高，左右黑边
        float scale = winW / viewW;
        float newHeight = viewH * scale;
        float offsetY = (winH - newHeight) * 0.5f;
        newView.setViewport(sf::FloatRect(sf::Vector2f(0.f, offsetY / winH), sf::Vector2f(1.f, newHeight / winH)));
    }
    window.setView(newView);
}

// ---------- 开始界面 ----------
class StartScreen
{
    sf::RenderWindow window;
    sf::View fixedView;
    sf::Font font;
    bool hasFont = false;
    std::filesystem::path dataRoot;
    sf::Texture backgroundTexture;
    std::optional<sf::Sprite> backgroundSprite;
    bool hasBackgroundTexture = false;
    sf::Texture buttonTexture;
    std::optional<sf::Sprite> buttonSprite;
    bool hasButtonTexture = false;
    sf::RectangleShape panel;
    sf::RectangleShape startButton;
    sf::Text titleText;
    sf::Text subtitleText;
    sf::Text buttonText;

public:
    StartScreen(const sf::Vector2u &windowSize = sf::Vector2u(1280, 720))
        : window(sf::VideoMode(windowSize, 32), "拆弹猫 - Start", sf::Style::Default),
          fixedView(sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(LOGIC_WIDTH, LOGIC_HEIGHT))),
          titleText(font),
          subtitleText(font),
          buttonText(font)
    {
        window.setFramerateLimit(60);
        window.setView(fixedView);

        // 如果上次是最大化，则最大化当前窗口
        if (g_wasMaximized)
        {
            maximizeWindow(window);
        }

        hasFont = loadSystemUIFont(font);
        dataRoot = findDataRoot();

        // 背景图片
        std::filesystem::path backgroundPath;
        if (!dataRoot.empty())
            backgroundPath = dataRoot / "bg" / "背景1.jpg";
        else
            backgroundPath = std::filesystem::path("data/bg") / "背景1.jpg";

        hasBackgroundTexture = loadTextureFromCandidates(backgroundTexture, makeCandidatePaths(backgroundPath));
        if (hasBackgroundTexture)
        {
            backgroundSprite.emplace(backgroundTexture);
            backgroundSprite->setTexture(backgroundTexture, true);
            const auto textureSize = backgroundTexture.getSize();
            if (textureSize.x > 0 && textureSize.y > 0)
            {
                float scaleX = LOGIC_WIDTH / static_cast<float>(textureSize.x);
                float scaleY = LOGIC_HEIGHT / static_cast<float>(textureSize.y);
                float scale = std::max(scaleX, scaleY);
                backgroundSprite->setScale(sf::Vector2f(scale, scale));
                float scaledWidth = textureSize.x * scale;
                float scaledHeight = textureSize.y * scale;
                backgroundSprite->setPosition(sf::Vector2f((LOGIC_WIDTH - scaledWidth) * 0.5f,
                                                           (LOGIC_HEIGHT - scaledHeight) * 0.5f));
            }
        }

        // 开始按钮图片
        std::filesystem::path buttonPath;
        if (!dataRoot.empty())
            buttonPath = dataRoot / "bg" / "begin.png";
        else
            buttonPath = std::filesystem::path("data/bg") / "begin.png";

        hasButtonTexture = loadTextureFromCandidates(buttonTexture, makeCandidatePaths(buttonPath));
        if (hasButtonTexture)
        {
            buttonSprite.emplace(buttonTexture);
            buttonSprite->setTexture(buttonTexture, true);
        }

        panel.setSize({420.f, 250.f});
        panel.setPosition({820.f, 50.f});
        panel.setFillColor(sf::Color(32, 38, 52, 190));
        panel.setOutlineThickness(2.f);
        panel.setOutlineColor(sf::Color(88, 96, 120));

        startButton.setSize({300.f, 96.f});
        startButton.setPosition({640.f - startButton.getSize().x * 0.5f, 560.f});
        startButton.setFillColor(sf::Color(52, 170, 109, 0));
        startButton.setOutlineThickness(0.f);
        startButton.setOutlineColor(sf::Color::Transparent);

        if (hasButtonTexture)
        {
            const auto textureSize = buttonTexture.getSize();
            if (textureSize.x > 0 && textureSize.y > 0)
            {
                float scaleX = startButton.getSize().x / static_cast<float>(textureSize.x);
                float scaleY = startButton.getSize().y / static_cast<float>(textureSize.y);
                float scale = std::min(scaleX, scaleY);
                buttonSprite->setScale(sf::Vector2f(scale, scale));
                float scaledWidth = textureSize.x * scale;
                float scaledHeight = textureSize.y * scale;
                float offsetX = startButton.getPosition().x + (startButton.getSize().x - scaledWidth) * 0.5f;
                float offsetY = startButton.getPosition().y + (startButton.getSize().y - scaledHeight) * 0.5f;
                buttonSprite->setPosition({offsetX, offsetY});
            }
            startButton.setFillColor(sf::Color::Transparent);
        }

        titleText.setCharacterSize(64);
        titleText.setStyle(sf::Text::Bold);
        titleText.setFillColor(sf::Color::White);
        titleText.setString(hasFont ? makeSfUtf8String(u8"拆弹猫") : sf::String("Bomb Cat"));

        subtitleText.setCharacterSize(30);
        subtitleText.setFillColor(sf::Color(230, 235, 245));
        subtitleText.setString(hasFont ? makeSfUtf8String(u8"准备开始一局紧张的拆弹对决") : sf::String("Press start to play"));

        buttonText.setCharacterSize(34);
        buttonText.setStyle(sf::Text::Bold);
        buttonText.setFillColor(sf::Color::White);
        buttonText.setString(sf::String());

        auto alignTextRight = [&](sf::Text &text, float rightX, float y)
        {
            const auto bounds = text.getLocalBounds();
            text.setOrigin(sf::Vector2f(bounds.position.x + bounds.size.x, bounds.position.y + bounds.size.y * 0.5f));
            text.setPosition(sf::Vector2f(rightX, y));
        };
        alignTextRight(titleText, 1220.f, 110.f);
        alignTextRight(subtitleText, 1220.f, 182.f);
    }

    bool run()
    {
        while (window.isOpen())
        {
            bool startRequested = false;
            while (const std::optional event = window.pollEvent())
            {
                if (event->is<sf::Event::Closed>())
                {
                    window.close();
                    return false;
                }
                if (event->is<sf::Event::Resized>())
                {
                    const auto resized = event->getIf<sf::Event::Resized>();
                    applyResizeView(window, fixedView, resized->size.x, resized->size.y);
                }
                if (event->is<sf::Event::KeyPressed>())
                {
                    const auto keyEvent = event->getIf<sf::Event::KeyPressed>();
                    if (keyEvent && keyEvent->code == sf::Keyboard::Key::Enter)
                        startRequested = true;
                }
                if (event->is<sf::Event::MouseButtonPressed>())
                {
                    const auto mouseEvent = event->getIf<sf::Event::MouseButtonPressed>();
                    if (mouseEvent && mouseEvent->button == sf::Mouse::Button::Left)
                    {
                        sf::Vector2f mousePos = window.mapPixelToCoords(mouseEvent->position);
                        if (startButton.getGlobalBounds().contains(mousePos))
                            startRequested = true;
                    }
                }
            }

            if (startRequested)
            {
                g_lastWindowSize = window.getSize();
                g_wasMaximized = isWindowMaximized(window);
                window.close();
                return true;
            }

            const sf::Vector2f mousePos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            if (!hasButtonTexture)
            {
                bool hovered = startButton.getGlobalBounds().contains(mousePos);
                startButton.setFillColor(hovered ? sf::Color(72, 196, 129) : sf::Color(52, 170, 109));
            }

            window.clear();
            if (hasBackgroundTexture)
                window.draw(*backgroundSprite);
            else
            {
                sf::RectangleShape fallbackBackground({LOGIC_WIDTH, LOGIC_HEIGHT});
                fallbackBackground.setFillColor(sf::Color(18, 22, 32));
                window.draw(fallbackBackground);
            }
            window.draw(panel);
            if (hasButtonTexture && buttonSprite.has_value())
                window.draw(*buttonSprite);
            window.draw(startButton);
            window.draw(titleText);
            window.draw(subtitleText);
            window.draw(buttonText);
            window.display();
        }
        return false;
    }
};

// ---------- 选择玩家数量界面 ----------
class PlayerSelectScreen
{
    sf::RenderWindow window;
    sf::View fixedView;
    sf::Font font;
    bool hasFont = false;
    std::filesystem::path dataRoot;
    sf::Texture bgTexture;
    std::optional<sf::Sprite> bgSprite;
    bool hasBg = false;
    std::vector<sf::RectangleShape> buttons;
    std::vector<sf::Text> labels;
    std::vector<sf::Texture> buttonTextures;
    std::vector<bool> hasButtonTexture;
    std::vector<std::optional<sf::Sprite>> buttonSprites;

public:
    PlayerSelectScreen(const sf::Vector2u &windowSize = sf::Vector2u(1280, 720))
        : window(sf::VideoMode(windowSize, 32), "选择玩家数量", sf::Style::Default),
          fixedView(sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(LOGIC_WIDTH, LOGIC_HEIGHT))),
          labels()
    {
        window.setFramerateLimit(60);
        window.setView(fixedView);

        if (g_wasMaximized)
        {
            maximizeWindow(window);
        }

        hasFont = loadSystemUIFont(font);
        dataRoot = findDataRoot();

        std::filesystem::path bgPath = dataRoot.empty() ? std::filesystem::path("data/bg") / "背景2.jpg" : dataRoot / "bg" / "背景2.jpg";
        hasBg = loadTextureFromCandidates(bgTexture, makeCandidatePaths(bgPath));
        if (hasBg)
        {
            bgSprite.emplace(bgTexture);
            const auto ts = bgTexture.getSize();
            if (ts.x > 0 && ts.y > 0)
            {
                float sx = LOGIC_WIDTH / static_cast<float>(ts.x);
                float sy = LOGIC_HEIGHT / static_cast<float>(ts.y);
                float s = std::max(sx, sy);
                bgSprite->setScale({s, s});
                float w = ts.x * s, h = ts.y * s;
                bgSprite->setPosition(sf::Vector2f((LOGIC_WIDTH - w) * 0.5f, (LOGIC_HEIGHT - h) * 0.5f));
            }
        }

        const float btnSize = 100.f;
        const float gap = 28.f;
        const float totalW = 3 * btnSize + 2 * gap;
        const float totalH = 2 * btnSize + gap;
        const float marginRight = 90.f;
        const float marginBottom = 80.f;
        const float startX = LOGIC_WIDTH - marginRight - totalW;
        const float startY = LOGIC_HEIGHT - marginBottom - totalH;

        buttonTextures.resize(6);
        hasButtonTexture.resize(6, false);
        for (int ti = 0; ti < 6; ++ti)
        {
            const std::string fname = std::to_string(ti + 1) + ".png";
            std::filesystem::path tpath = dataRoot.empty() ? std::filesystem::path("data/bg") / fname : dataRoot / "bg" / fname;
            hasButtonTexture[ti] = loadTextureFromCandidates(buttonTextures[ti], makeCandidatePaths(tpath));
        }

        for (int r = 0; r < 2; ++r)
        {
            for (int c = 0; c < 3; ++c)
            {
                const int idx = r * 3 + c;
                sf::RectangleShape btn({btnSize, btnSize});
                btn.setPosition(sf::Vector2f(startX + c * (btnSize + gap), startY + r * (btnSize + gap)));
                if (hasButtonTexture[idx])
                {
                    btn.setFillColor(sf::Color::Transparent);
                    btn.setOutlineThickness(0.f);
                }
                else
                {
                    btn.setFillColor(sf::Color(70, 130, 180, 200));
                    btn.setOutlineColor(sf::Color::White);
                    btn.setOutlineThickness(2.f);
                }
                buttons.push_back(btn);

                if (hasButtonTexture[idx])
                {
                    buttonSprites.emplace_back(sf::Sprite(buttonTextures[idx]));
                    auto &spr = *buttonSprites.back();
                    const auto ts = buttonTextures[idx].getSize();
                    if (ts.x > 0 && ts.y > 0)
                    {
                        const float sx = btnSize / static_cast<float>(ts.x);
                        const float sy = btnSize / static_cast<float>(ts.y);
                        spr.setScale({sx, sy});
                    }
                    spr.setPosition(sf::Vector2f(startX + c * (btnSize + gap), startY + r * (btnSize + gap)));
                }
                else
                {
                    buttonSprites.emplace_back(std::nullopt);
                }

                sf::Text t(font);
                t.setCharacterSize(18);
                t.setFillColor(sf::Color::White);
                labels.push_back(t);
            }
        }

        for (size_t i = 0; i < labels.size(); ++i)
        {
            const std::string s = std::to_string(static_cast<int>(i + 1));
            labels[i].setFont(font);
            labels[i].setString(makeSfUtf8String(s));
            const auto bpos = buttons[i].getPosition();
            const auto bsize = buttons[i].getSize();
            const auto lb = labels[i].getLocalBounds();
            labels[i].setOrigin(sf::Vector2f(lb.position.x + lb.size.x * 0.5f, lb.position.y + lb.size.y * 0.5f));
            labels[i].setPosition(sf::Vector2f(bpos.x + bsize.x * 0.5f, bpos.y + bsize.y * 0.5f - 3.f));
        }
    }

    int run()
    {
        while (window.isOpen())
        {
            while (const std::optional ev = window.pollEvent())
            {
                if (ev->is<sf::Event::Closed>())
                {
                    window.close();
                    return 0;
                }
                if (ev->is<sf::Event::Resized>())
                {
                    const auto resized = ev->getIf<sf::Event::Resized>();
                    applyResizeView(window, fixedView, resized->size.x, resized->size.y);
                }
                if (ev->is<sf::Event::MouseButtonPressed>())
                {
                    const auto me = ev->getIf<sf::Event::MouseButtonPressed>();
                    if (me && me->button == sf::Mouse::Button::Left)
                    {
                        sf::Vector2f mp = window.mapPixelToCoords(me->position);
                        for (size_t i = 0; i < buttons.size(); ++i)
                        {
                            if (buttons[i].getGlobalBounds().contains(mp))
                            {
                                g_lastWindowSize = window.getSize();
                                g_wasMaximized = isWindowMaximized(window);
                                window.close();
                                return static_cast<int>(i + 1);
                            }
                        }
                    }
                }
            }

            window.clear();
            if (hasBg)
                window.draw(*bgSprite);
            else
            {
                sf::RectangleShape fb({LOGIC_WIDTH, LOGIC_HEIGHT});
                fb.setFillColor(sf::Color(18, 22, 32));
                window.draw(fb);
            }

            for (size_t i = 0; i < buttons.size(); ++i)
            {
                if (i < hasButtonTexture.size() && hasButtonTexture[i])
                {
                    if (i < buttonSprites.size() && buttonSprites[i].has_value())
                        window.draw(*buttonSprites[i]);
                    else
                        window.draw(buttons[i]);
                }
                else
                {
                    window.draw(buttons[i]);
                }
            }
            for (size_t i = 0; i < labels.size(); ++i)
            {
                if (i < hasButtonTexture.size() && hasButtonTexture[i])
                    continue;
                window.draw(labels[i]);
            }

            window.display();
        }
        return 0;
    }
};

// ---------- 游戏主界面 ----------
class UIManager
{
    sf::RenderWindow window;
    sf::View fixedView;
    sf::Font font;
    Game &game;
    std::filesystem::path dataRoot;
    std::array<sf::Texture, 11> cardTextures;
    std::array<bool, 11> cardTextureLoaded;
    sf::Texture deckTexture;
    std::optional<sf::Sprite> deckSprite;
    bool deckTextureLoaded = false;

    sf::RectangleShape deckShape;
    sf::Text deckLabel;

    std::vector<sf::RectangleShape> handShapes;
    std::vector<sf::Sprite> handSprites;
    std::vector<sf::Text> handLabels;

    std::vector<sf::RectangleShape> targetButtons;
    std::vector<sf::Text> targetLabels;
    std::vector<int> targetPlayerIndexes;

    sf::Text messageText;
    sf::Text currentPlayerText;
    sf::Text timerText;

public:
    UIManager(Game &g, const sf::Vector2u &windowSize = sf::Vector2u(1280, 720))
        : window(sf::VideoMode(windowSize, 32), "拆弹猫 - 简约版", sf::Style::Default),
          fixedView(sf::FloatRect(sf::Vector2f(0.f, 0.f), sf::Vector2f(LOGIC_WIDTH, LOGIC_HEIGHT))),
          game(g),
          dataRoot(findDataRoot()),
          cardTextureLoaded(),
          deckLabel(font),
          messageText(font),
          currentPlayerText(font),
          timerText(font)
    {
        window.setFramerateLimit(60);
        window.setView(fixedView);

        if (g_wasMaximized)
        {
            maximizeWindow(window);
        }

        cardTextureLoaded.fill(false);

        bool fontLoaded = false;
        std::vector<std::string> fontPaths = {
            "C:\\Windows\\Fonts\\msyh.ttc",
            "C:\\Windows\\Fonts\\simhei.ttf",
            "C:\\Windows\\Fonts\\simsun.ttc"};

        for (const auto &path : fontPaths)
        {
            if (font.openFromFile(path))
            {
                fontLoaded = true;
                break;
            }
        }
        if (!fontLoaded)
        {
            std::cerr << "ERROR: 字体加载失败！" << std::endl;
            exit(-1);
        }

        deckShape.setSize({140, 190});
        deckShape.setPosition({550, 260});
        deckShape.setFillColor(sf::Color(139, 69, 19));

        std::filesystem::path deckImagePath;
        if (!dataRoot.empty())
            deckImagePath = dataRoot / "pai" / "mopaidui.png";
        else
            deckImagePath = std::filesystem::path("data/pai") / "mopaidui.png";

        deckTextureLoaded = loadTextureFromCandidates(deckTexture, makeCandidatePaths(deckImagePath));
        if (deckTextureLoaded)
        {
            deckShape.setFillColor(sf::Color::Transparent);
            deckSprite.emplace(deckTexture);
            const auto textureSize = deckTexture.getSize();
            if (textureSize.x > 0 && textureSize.y > 0)
            {
                const float sx = deckShape.getSize().x / static_cast<float>(textureSize.x);
                const float sy = deckShape.getSize().y / static_cast<float>(textureSize.y);
                const float uniformScale = std::min(sx, sy);
                deckSprite->setScale(sf::Vector2f(uniformScale, uniformScale));

                const float scaledWidth = static_cast<float>(textureSize.x) * uniformScale;
                const float scaledHeight = static_cast<float>(textureSize.y) * uniformScale;
                const float offsetX = (deckShape.getSize().x - scaledWidth) * 0.5f;
                const float offsetY = (deckShape.getSize().y - scaledHeight) * 0.5f;
                deckSprite->setPosition({deckShape.getPosition().x + offsetX, deckShape.getPosition().y + offsetY});
            }
            else
            {
                deckSprite->setPosition(deckShape.getPosition());
            }
        }

        deckLabel = sf::Text(font, makeSfUtf8String(std::string(u8"\u6478\u724c\u5806")), 18);
        deckLabel.setFillColor(sf::Color::White);
        deckLabel.setOutlineColor(sf::Color(0, 0, 0, 180));
        deckLabel.setOutlineThickness(2.f);

        messageText = sf::Text(font, sf::String(), 24);
        messageText.setFillColor(sf::Color::White);
        messageText.setPosition({50.f, 50.f});

        currentPlayerText = sf::Text(font, sf::String(), 30);
        currentPlayerText.setFillColor(sf::Color::Yellow);
        currentPlayerText.setPosition({50.f, 100.f});

        timerText = sf::Text(font, sf::String(), 24);
        timerText.setFillColor(sf::Color(255, 215, 0));
        timerText.setPosition({50.f, 140.f});

        const std::vector<CardType> texturedTypes = {
            CardType::Defuse,
            CardType::SeeTheFuture,
            CardType::Prophecy,
            CardType::Shuffle,
            CardType::DrawFromBottom,
            CardType::Skip,
            CardType::Reverse,
            CardType::Attack,
            CardType::Favor,
            CardType::Exchange};

        for (CardType type : texturedTypes)
        {
            std::string filename = cardTextureFilename(type);
            if (filename.empty())
                continue;

            std::filesystem::path imagePath;
            if (!dataRoot.empty())
                imagePath = dataRoot / "pai" / filename;
            else
                imagePath = std::filesystem::path("data/pai") / filename;

            if (loadTextureFromCandidates(cardTextures[static_cast<size_t>(type)], makeCandidatePaths(imagePath)))
                cardTextureLoaded[static_cast<size_t>(type)] = true;
        }
    }

    bool hasCardTexture(CardType type) const
    {
        return cardTextureLoaded[static_cast<size_t>(type)];
    }

    void updateHandDisplay()
    {
        handShapes.clear();
        handSprites.clear();
        handLabels.clear();
        Player *player = game.getCurrentPlayer();
        if (!player || !player->alive)
            return;

        const float startX = 80.f;
        const float startY = 520.f;
        const float cardWidth = 140.f;
        const float cardHeight = 190.f;
        const float gap = 16.f;
        const int cardsPerRow = std::max(1, static_cast<int>((LOGIC_WIDTH - startX * 2 + gap) / (cardWidth + gap)));

        for (size_t i = 0; i < player->hand.size(); ++i)
        {
            int row = static_cast<int>(i) / cardsPerRow;
            int column = static_cast<int>(i) % cardsPerRow;
            float x = startX + column * (cardWidth + gap);
            float y = startY + row * (cardHeight + gap);

            sf::RectangleShape rect({cardWidth, cardHeight});
            rect.setPosition({x, y});
            rect.setFillColor(hasCardTexture(player->hand[i].type) ? sf::Color::Transparent : cardColor(player->hand[i].type));
            rect.setOutlineThickness(2.f);
            rect.setOutlineColor(sf::Color(30, 30, 30));
            handShapes.push_back(rect);

            if (hasCardTexture(player->hand[i].type))
            {
                sf::Sprite sprite(cardTextures[static_cast<size_t>(player->hand[i].type)]);
                const auto textureSize = cardTextures[static_cast<size_t>(player->hand[i].type)].getSize();
                if (textureSize.x > 0 && textureSize.y > 0)
                {
                    const float sx = cardWidth / static_cast<float>(textureSize.x);
                    const float sy = cardHeight / static_cast<float>(textureSize.y);
                    const float uniformScale = std::min(sx, sy);
                    sprite.setScale(sf::Vector2f(uniformScale, uniformScale));

                    const float scaledWidth = static_cast<float>(textureSize.x) * uniformScale;
                    const float scaledHeight = static_cast<float>(textureSize.y) * uniformScale;
                    const float offsetX = (cardWidth - scaledWidth) * 0.5f;
                    const float offsetY = (cardHeight - scaledHeight) * 0.5f;
                    sprite.setPosition({x + offsetX, y + offsetY});
                }
                else
                {
                    sprite.setPosition({x, y});
                }
                handSprites.push_back(sprite);
            }

            if (!hasCardTexture(player->hand[i].type))
            {
                sf::Text label(font, makeSfUtf8String(cardName(player->hand[i].type)), 18);
                label.setFillColor(sf::Color::White);
                label.setPosition({x + 8.f, y + cardHeight - 28.f});
                handLabels.push_back(label);
            }
        }
    }

    void updateTargetDisplay()
    {
        targetButtons.clear();
        targetLabels.clear();
        targetPlayerIndexes.clear();

        if (!game.isWaitingForTarget())
            return;

        std::vector<int> targets = game.getSelectableTargets();
        const float startX = 100.f;
        const bool showTargetHandCount = (game.pendingAction == Game::PendingAction::Favor ||
                                          game.pendingAction == Game::PendingAction::Exchange ||
                                          game.pendingAction == Game::PendingAction::Attack);
        const float y = showTargetHandCount ? 460.f : 470.f;
        const float btnWidth = 120.f;
        const float btnHeight = 44.f;
        const float gap = 12.f;

        for (size_t i = 0; i < targets.size(); ++i)
        {
            int playerIndex = targets[i];
            float x = startX + static_cast<float>(i) * (btnWidth + gap);

            sf::RectangleShape btn({btnWidth, btnHeight});
            btn.setPosition({x, y});
            btn.setFillColor(sf::Color(70, 70, 130));
            targetButtons.push_back(btn);

            std::string displayName = game.players[playerIndex].name;
            if (showTargetHandCount)
            {
                displayName += std::string(u8" （") + std::to_string(static_cast<int>(game.players[playerIndex].hand.size())) + std::string(u8"张）");
            }
            sf::Text label(font, makeSfUtf8String(displayName), 18);
            label.setFillColor(sf::Color::White);
            label.setPosition({x + 12.f, y + 10.f});
            targetLabels.push_back(label);

            targetPlayerIndexes.push_back(playerIndex);
        }
    }

    void processEvents()
    {
        while (auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();
            else if (event->is<sf::Event::Resized>())
            {
                const auto resized = event->getIf<sf::Event::Resized>();
                applyResizeView(window, fixedView, resized->size.x, resized->size.y);
            }
            else if (event->is<sf::Event::MouseButtonPressed>())
            {
                auto pos = event->getIf<sf::Event::MouseButtonPressed>()->position;
                sf::Vector2f mousePos = window.mapPixelToCoords(pos);

                if (game.isWaitingForTarget())
                {
                    updateTargetDisplay();
                    for (size_t i = 0; i < targetButtons.size(); ++i)
                    {
                        if (targetButtons[i].getGlobalBounds().contains(mousePos))
                        {
                            game.selectTarget(targetPlayerIndexes[i]);
                            break;
                        }
                    }
                }

                if (!game.isWaitingForTarget())
                {
                    for (size_t i = 0; i < handShapes.size(); ++i)
                    {
                        if (handShapes[i].getGlobalBounds().contains(mousePos))
                        {
                            game.playCard(static_cast<int>(i));
                            break;
                        }
                    }
                }

                if (deckShape.getGlobalBounds().contains(mousePos) && game.canDrawNow())
                {
                    game.endPlayPhase();
                }
            }
            else if (event->is<sf::Event::KeyPressed>())
            {
                if (event->getIf<sf::Event::KeyPressed>()->code == sf::Keyboard::Key::Space)
                {
                    if (game.message.empty())
                        game.nextTurn();
                }
            }
        }
    }

    void render()
    {
        window.clear(sf::Color(30, 30, 30));

        auto centerText = [](sf::Text &text)
        {
            const auto bounds = text.getLocalBounds();
            text.setOrigin({bounds.position.x + bounds.size.x / 2.f, bounds.position.y + bounds.size.y / 2.f});
        };

        window.draw(deckShape);
        if (deckTextureLoaded && deckSprite.has_value())
            window.draw(*deckSprite);

        size_t remaining = game.deck.size();
        sf::Text remainingText(font, makeSfUtf8String(std::string(u8"剩余 ") + std::to_string(remaining) + std::string(u8" 张")), 18);
        remainingText.setFillColor(sf::Color::White);
        remainingText.setOutlineColor(sf::Color(0, 0, 0, 180));
        remainingText.setOutlineThickness(1.f);
        remainingText.setPosition({deckShape.getPosition().x + deckShape.getSize().x / 2.f, deckShape.getPosition().y + deckShape.getSize().y - 48.f});
        centerText(remainingText);
        window.draw(remainingText);

        int bombs = 0;
        for (const auto &c : game.deck.getCards())
            if (c.type == CardType::ExplodingKitten)
                ++bombs;

        float prob = 0.f;
        if (game.deck.size() > 0)
            prob = static_cast<float>(bombs) / static_cast<float>(game.deck.size());

        const float barWidth = 140.f;
        const float barHeight = 16.f;
        sf::Vector2f barPos(deckShape.getPosition().x + deckShape.getSize().x + 6.f, deckShape.getPosition().y + deckShape.getSize().y / 2.f - barHeight / 2.f);

        sf::RectangleShape barBg({barWidth, barHeight});
        barBg.setPosition(barPos);
        barBg.setFillColor(sf::Color(50, 50, 50));
        window.draw(barBg);

        sf::RectangleShape barFill({barWidth * prob, barHeight});
        barFill.setPosition(barPos);
        std::uint8_t red = static_cast<std::uint8_t>(200 + 55 * prob);
        barFill.setFillColor(sf::Color(red, 40, 40));
        window.draw(barFill);

        int pct = static_cast<int>(prob * 100.0f + 0.5f);
        sf::Text probText(font, makeSfUtf8String(std::string(u8"爆炸概率: ") + std::to_string(pct) + std::string(u8"%")), 14);
        probText.setFillColor(sf::Color::White);
        probText.setPosition({barPos.x, barPos.y - 18.f});
        window.draw(probText);

        updateHandDisplay();
        for (auto &shape : handShapes)
            window.draw(shape);
        for (auto &sprite : handSprites)
            window.draw(sprite);
        for (auto &text : handLabels)
            window.draw(text);

        updateTargetDisplay();
        for (auto &btn : targetButtons)
            window.draw(btn);
        for (auto &label : targetLabels)
            window.draw(label);

        if (!game.message.empty())
            messageText.setString(makeSfUtf8String(game.message));
        else
            messageText.setString(sf::String());
        window.draw(messageText);

        currentPlayerText.setString(makeSfUtf8String(std::string(u8"\u5f53\u524d\uff1a") + game.getCurrentPlayer()->name +
                                                     (game.getCurrentPlayer()->alive ? "" : std::string(u8" (\u5df2\u6dd8\u6c70)"))));
        window.draw(currentPlayerText);

        int remain = game.getRemainingDrawSeconds();
        if (remain >= 0)
        {
            timerText.setString(makeSfUtf8String(std::string(u8"\u6478\u724c\u5012\u8ba1\u65f6\uff1a") + std::to_string(remain) + std::string(u8" \u79d2")));
            window.draw(timerText);
        }

        window.display();
    }

    void run()
    {
        while (window.isOpen())
        {
            processEvents();
            game.update();
            if (game.gameOver)
            {
                Player *winner = game.getWinner();
                std::string winnerName = winner ? winner->name : std::string(u8"\u672a\u77e5");
                messageText.setString(makeSfUtf8String(std::string(u8"\u6e38\u620f\u7ed3\u675f\uff01\u80dc\u5229\u8005\uff1a") + winnerName));
                window.clear();
                window.draw(messageText);
                window.display();
                sf::sleep(sf::seconds(3));
                g_lastWindowSize = window.getSize();
                g_wasMaximized = isWindowMaximized(window);
                window.close();
                break;
            }
            render();
        }
    }
};

int main()
{
#ifdef _WIN32
    system("chcp 65001 > nul");
#endif
    std::setlocale(LC_ALL, ".UTF-8");

    StartScreen startScreen(g_lastWindowSize);
    if (!startScreen.run())
        return 0;

    PlayerSelectScreen psel(g_lastWindowSize);
    int numPlayers = psel.run();
    if (numPlayers <= 0)
        return 0;

    Game game(numPlayers);
    UIManager ui(game, g_lastWindowSize);
    ui.run();

    return 0;
}