#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
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
#include <sstream>
#include <string>
#include <vector>
#include <array>
#include <cstdint>

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
    // 爆炸猫：抽到后若无拆除牌则立即出局
    case CardType::ExplodingKitten:
        return u8"\u70b8\u5f39";
    // 拆除：用于化解爆炸猫并将其放回牌库
    case CardType::Defuse:
        return u8"\u62c6\u9664";
    // 透视：查看牌库顶的三张牌
    case CardType::SeeTheFuture:
        return u8"\u900f\u89c6";
    // 预言：提示最近一张炸弹距离牌顶的位置
    case CardType::Prophecy:
        return u8"\u9884\u8a00";
    // 洗牌：将当前牌库顺序随机打乱
    case CardType::Shuffle:
        return u8"\u6d17\u724c";
    // 抽底：下一次抽牌改为从牌库底部抽取
    case CardType::DrawFromBottom:
        return u8"\u62bd\u5e95";
    // 跳过：直接结束自己的当前回合（不抽牌）
    case CardType::Skip:
        return u8"\u8df3\u8fc7";
    // 转向：反转出牌/回合推进方向
    case CardType::Reverse:
        return u8"\u8f6c\u5411";
    // 甩锅：令目标玩家额外执行一个完整回合
    case CardType::Attack:
        return u8"\u7529\u9505";
    // 索要：从目标玩家手中随机拿走一张牌
    case CardType::Favor:
        return u8"\u7d22\u8981";
    // 交换：与指定目标交换全部手牌
    case CardType::Exchange:
        return u8"\u4ea4\u6362";
    // 未知：兜底分支，正常情况下不应出现
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
    int forcedTargetIndex = -1;            // 当前被强制代摸的玩家索引（用于 Attack）
    int returnToPlayer = -1;               // Attack 后应回到的玩家索引
    bool returnPending = false;            // 是否在等待将回合交还给 returnToPlayer
    int attackOriginIndex = -1;            // 发起甩锅的玩家索引
    int attackOriginStep = 1;              // 发起甩锅时的方向步长（1 或 -1），用于按原始顺序查找下一位
    int attackChainSource = -1;            // 甩锅链的发起者索引（-1表示无链，用于限制被甩玩家再甩）
    bool waitingForAttackResponse = false; // 被甩玩家是否正在等待反击（可出牌或摸牌）
    PendingAction pendingAction = PendingAction::None;
    std::string message;
    // 抽牌后短暂停顿（以秒为单位）
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

        // 默认第一个玩家为人类，其他为 AI
        if (!players.empty())
            players[0].isAI = false;
        for (size_t i = 1; i < players.size(); ++i)
            players[i].isAI = true;

        currentPlayer = 0; // 开局固定为玩家一
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

    // 获取最后存活的玩家（胜者）
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

    // 在指定起点按当前方向寻找下一个存活玩家（排除起点自身）
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

        // 如果之前有一个被强制代摸的流程正在进行，且当前刚完成的是被代摸的玩家
        if (returnPending && forcedTargetIndex >= 0 && currentPlayer == forcedTargetIndex)
        {
            // 尝试按发起甩锅时的原始顺序寻找下一位存活玩家
            int candidate = -1;
            if (attackOriginIndex >= 0)
            {
                candidate = findNextAliveFrom(attackOriginIndex, attackOriginStep);
                // 如果找到的候选与发起者相同且该发起者已被淘汰，表示没有其他玩家，直接设置为 candidate
            }

            // 如果 candidate 无效或对应玩家不存活，则按当前方向从当前索引前进寻找存活玩家
            if (candidate < 0 || !players[candidate].alive)
            {
                int step = direction ? 1 : -1;
                candidate = findNextAliveFrom(currentPlayer, step);
            }

            currentPlayer = candidate;
            // 清理攻击相关状态
            returnPending = false;
            forcedNextPlayer = -1;
            forcedTargetIndex = -1;
            returnToPlayer = -1;
            attackOriginIndex = -1;
            attackOriginStep = 1;
            attackChainSource = -1; // 甩锅链结束，清除源

            // 启动该玩家的回合计时
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
                targets.push_back(i); // 甩锅可指定任意存活玩家（含自己）
                continue;
            }

            if (i != currentPlayer)
                targets.push_back(i);
        }
        return targets;
    }

    // AI 选择攻击目标：优先选择人类玩家（非 AI），若无则随机
    int chooseAIAttackTarget(int aiIndex)
    {
        // 首先找人类玩家
        for (int i = 0; i < static_cast<int>(players.size()); ++i)
        {
            if (i == aiIndex)
                continue;
            if (!players[i].alive)
                continue;
            if (!players[i].isAI)
                return i;
        }
        // 否则随机一个存活的玩家（非自己）
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

    // AI 为索要/交换选择手牌最多的玩家；并列时随机挑选
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

    // AI 自动目标选择待处理（用于在展示出牌后再自动选择目标以便显示中间结果）
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
                // 指定自己：不再授予额外回合，仅提示已指定自己（无效果）
                message = actor.name + std::string(u8" \u5bf9\u81ea\u5df1\u7529\u9505\uff1a\u65e0\u989d\u5916\u56de\u5408");
            }
            else
            {
                int attacker = currentPlayer;
                int step = direction ? 1 : -1;

                // 检查是否已在甩锅链中
                if (attackOriginIndex < 0)
                {
                    // 首次甩锅，记录原始发起者和方向
                    attackOriginIndex = attacker;
                    attackOriginStep = step;
                }
                // 如果已在甩锅链中，保持原发起者和方向不变，只更新被摸的玩家

                // 更新被甩的玩家（摸牌的玩家）
                forcedTargetIndex = targetIndex;
                forcedNextPlayer = targetIndex; // 立即把目标设为下个当前玩家去代摸
                // 设置甩锅链源为首次发起者，限制原始发起者被甩回时不能再甩
                attackChainSource = attackOriginIndex;

                // 仅在首次甩锅时计算回到的玩家
                if (returnToPlayer < 0)
                {
                    int nextAfterOrigin = findNextAliveFrom(attackOriginIndex, attackOriginStep);
                    returnToPlayer = nextAfterOrigin; // 代摸完成后应回到此存活玩家
                }

                waitingForAttackResponse = true;
                drawRequired = false;
                forcedNextPlayer = targetIndex;
                currentPlayer = targetIndex; // 切到被甩玩家
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
                // 如果有 AI 待选目标，优先执行选目标动作以展示出牌结果
                if (aiWillSelectTarget)
                {
                    aiWillSelectTarget = false;
                    int tgt = aiPendingTarget;
                    aiPendingTarget = -1;
                    selectTarget(tgt);
                    // 展示选定目标的结果
                    drawPaused = true;
                    drawPauseSeconds = 3.f;
                    drawPauseClock.restart();
                    return;
                }
                // 如果还需要摸牌（drawRequired为true）
                if (drawRequired)
                {
                    // AI且存活：自动结束出牌阶段并摸牌
                    if (!gameOver && players[currentPlayer].isAI && players[currentPlayer].alive)
                    {
                        endPlayPhase();
                        drawPaused = true;
                        drawPauseSeconds = 5.f;
                        drawPauseClock.restart();
                        return;
                    }

                    // 当前玩家已出局：直接推进到下一回合
                    if (!players[currentPlayer].alive)
                    {
                        nextTurn();
                        return;
                    }

                    // 否则是人类玩家且仍需摸牌：等待玩家手动操作（不自动推进）
                    return;
                }

                // drawRequired 为 false，进入下一回合
                nextTurn();
            }
            return;
        }

        // 如果当前玩家是 AI，且可以执行出牌/抽牌操作，则让 AI 决策
        if (!gameOver && players[currentPlayer].isAI && players[currentPlayer].alive && !isWaitingForTarget())
        {
            // 如果处于等待反击状态，AI 可以选择反击或摸牌
            if (waitingForAttackResponse && currentPlayer == forcedTargetIndex)
            {
                // 优先甩锅，其次跳过/转向，否则摸牌（由 endPlayPhase 处理）
                Player &ai = players[currentPlayer];
                int idxAttack = canAIUseAttackNow(currentPlayer) ? ai.findCardIndex(CardType::Attack) : -1;
                if (idxAttack >= 0)
                {
                    playCard(idxAttack);
                    // 明确显示 AI 出了甩锅牌
                    message = ai.name + std::string(u8" 出了 ") + cardName(CardType::Attack);
                    // AI 在展示出牌后再自动选择目标以便用户能看到出牌信息
                    int tgt = chooseAIAttackTarget(currentPlayer);
                    if (tgt >= 0)
                    {
                        aiPendingTarget = tgt;
                        aiWillSelectTarget = true;
                    }
                    // 展示短暂停顿（显示出牌）
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

                // 无牌可抵抗，摸牌
                endPlayPhase();
                // 将自动在 handleDrawnCard 中设置停顿（我们希望 AI 的停顿为3秒）
                drawPaused = true;
                drawPauseSeconds = 3.f;
                drawPauseClock.restart();
                return;
            }

            // 普通回合：优先使用功能性卡牌（Skip, Reverse, Favor, Exchange），否则抽牌
            if (drawRequired)
            {
                Player &ai = players[currentPlayer];
                // 安全策略：不主动打出 Defuse
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
                        // 明确显示 AI 出了哪张牌
                        message = ai.name + std::string(u8" 出了 ") + cardName(ct);
                        // 如果是需要选择目标的牌，AI 延迟选择目标以先展示出牌动作
                        if (ct == CardType::Favor || ct == CardType::Exchange || ct == CardType::Attack)
                        {
                            int target = -1;
                            if (ct == CardType::Favor || ct == CardType::Exchange)
                                target = chooseAITargetForStealOrExchange(currentPlayer);
                            else if (ct == CardType::Attack)
                                target = chooseAIAttackTarget(currentPlayer);
                            if (target >= 0)
                            {
                                // 延迟选择目标以先展示 AI 出牌动作
                                aiPendingTarget = target;
                                aiWillSelectTarget = true;
                            }
                        }
                        // AI 出牌后显示短暂停顿
                        drawPaused = true;
                        drawPauseSeconds = 3.f;
                        drawPauseClock.restart();
                        return;
                    }
                }

                // 没有可出的功能牌，摸牌
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

        // 如果正在等待反击，结束反击等待状态并摸牌
        if (waitingForAttackResponse)
        {
            message = getCurrentPlayer()->name + std::string(u8" 选择摸牌，甩锅链结束");
            waitingForAttackResponse = false;
            // 关键：被甩玩家要摸牌了，设置returnPending为true
            // 这样摸完牌后在nextTurn()中会回到原始甩锅者的下家
            returnPending = true;
            // 暂时保留forcedTargetIndex, attackOriginIndex等信息，用于nextTurn()检查
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
                // 摸牌后短暂停顿
                drawPaused = true;
                drawPauseSeconds = 5.f;
                drawPauseClock.restart();
                // 已经摸过牌，设置标志防止再摸
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
                    // 玩家被炸出局后短暂停顿 5 秒，随后推进到下一个存活玩家
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

        // 普通摸牌后短暂停顿 5 秒以展示信息
        drawPaused = true;
        drawPauseSeconds = 5.f;
        drawPauseClock.restart();
        // 已经摸过牌，设置标志防止再摸
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

        // 当玩家正在等待攻击反击时，只允许出特定的牌
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

        // 只有在甩锅链源是当前玩家时，才禁止出甩锅牌（表示被甩回了）
        if (attackChainSource >= 0 && attackChainSource == currentPlayer && card.type == CardType::Attack)
        {
            // 甩锅无效，把牌加回手中
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
                // 保留甩锅链信息，交给 nextTurn() 按原始甩锅发起者的下家回收回合
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
                // 反击甩锅，继续链传递
                pendingAction = PendingAction::Attack;
                message = p.name + std::string(u8" 反击甩锅！请选择甩锅目标");
            }
            else if (attackChainSource >= 0 && attackChainSource == currentPlayer)
            {
                // 甩锅无效，把牌加回手中
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

// ---------- UI 定义（SFML 3 兼容版，使用系统字体）----------
class UIManager
{
    sf::RenderWindow window;
    sf::Font font;
    Game &game;
    std::filesystem::path dataRoot;
    std::array<sf::Texture, 11> cardTextures;
    std::array<bool, 11> cardTextureLoaded;

    sf::RectangleShape deckShape;
    sf::Text deckLabel;

    std::vector<sf::RectangleShape> handShapes;
    std::vector<sf::Sprite> handSprites;
    std::vector<sf::Text> handLabels; // 每个手牌文字也需要字体

    std::vector<sf::RectangleShape> targetButtons;
    std::vector<sf::Text> targetLabels;
    std::vector<int> targetPlayerIndexes;

    sf::Text messageText;
    sf::Text currentPlayerText;
    sf::Text timerText;

public:
    UIManager(Game &g)
        : window(sf::VideoMode({1200, 800}), "拆弹猫 - 简约版"),
          game(g),
          dataRoot(findDataRoot()),
          cardTextureLoaded(),
          deckLabel(font),
          messageText(font),
          currentPlayerText(font),
          timerText(font)
    {
        cardTextureLoaded.fill(false);

        // 使用系统自带中文字体（支持中文）
        // 尝试多个字体路径，优先级：微软雅黑 > 宋体 > 黑体
        bool fontLoaded = false;

        std::ofstream logFile("font_debug.log");
        logFile << "=== 字体加载调试 ===" << std::endl;

        std::vector<std::string> fontPaths = {
            "C:\\Windows\\Fonts\\msyh.ttc",
            "C:\\Windows\\Fonts\\simhei.ttf",
            "C:\\Windows\\Fonts\\simsun.ttc"};

        for (const auto &path : fontPaths)
        {
            logFile << "尝试加载: " << path << std::endl;
            if (font.openFromFile(path))
            {
                logFile << "✓ 成功: " << path << std::endl;
                std::cerr << "✓ 字体加载成功: " << path << std::endl;
                fontLoaded = true;
                break;
            }
            logFile << "✗ 失败: " << path << std::endl;
        }

        if (!fontLoaded)
        {
            logFile << "ERROR: 所有字体加载失败！" << std::endl;
            logFile.close();
            std::cerr << "ERROR: 所有字体加载失败！请检查Windows\\Fonts目录。" << std::endl;
            exit(-1);
        }

        logFile << "字体加载完成。" << std::endl;
        logFile.close();

        // 牌库图形
        deckShape.setSize({100, 140});
        deckShape.setPosition({550, 300});
        deckShape.setFillColor(sf::Color(139, 69, 19));

        // 字体加载成功后，重新创建Text对象以确保使用正确的字体
        deckLabel = sf::Text(font, makeSfUtf8String(std::string(u8"\u724c\u5e93")), 20);
        deckLabel.setFillColor(sf::Color::White);
        deckLabel.setPosition({570.f, 350.f});

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
        const int cardsPerRow = std::max(1, static_cast<int>((window.getSize().x - static_cast<unsigned int>(startX * 2) + static_cast<unsigned int>(gap)) / (cardWidth + gap)));

        for (size_t i = 0; i < player->hand.size(); ++i)
        {
            int row = static_cast<int>(i) / cardsPerRow;
            int column = static_cast<int>(i) % cardsPerRow;
            float x = startX + column * (cardWidth + gap);
            float y = startY + row * (cardHeight + gap);

            // 卡牌背景
            sf::RectangleShape rect({cardWidth, cardHeight});
            rect.setPosition({x, y});
            rect.setFillColor(hasCardTexture(player->hand[i].type) ? sf::Color(255, 255, 255, 30) : cardColor(player->hand[i].type));
            rect.setOutlineThickness(2.f);
            rect.setOutlineColor(sf::Color(30, 30, 30));
            handShapes.push_back(rect);

            if (hasCardTexture(player->hand[i].type))
            {
                sf::Sprite sprite(cardTextures[static_cast<size_t>(player->hand[i].type)]);
                const auto textureSize = cardTextures[static_cast<size_t>(player->hand[i].type)].getSize();
                if (textureSize.x > 0 && textureSize.y > 0)
                {
                    // Preserve source aspect ratio to avoid deformation on different image sizes.
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

            // 卡牌文字（✅ 必须带字体构造）
            sf::Text label(font, makeSfUtf8String(cardName(player->hand[i].type)), 18);
            label.setFillColor(sf::Color::White);
            label.setPosition({x + 8.f, y + cardHeight - 28.f});
            handLabels.push_back(label);
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
        const float y = 470.f;
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

            // 如果正在进行索要(Favor)或交换(Exchange)，在玩家名后显示当前手牌数量
            std::string displayName = game.players[playerIndex].name;
            if (game.pendingAction == Game::PendingAction::Favor || game.pendingAction == Game::PendingAction::Exchange)
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
            else if (event->is<sf::Event::MouseButtonPressed>())
            {
                auto pos = event->getIf<sf::Event::MouseButtonPressed>()->position;
                // 将像素坐标映射到当前视图坐标，避免窗口缩放/最大化后命中检测偏移。
                sf::Vector2f mousePos = window.mapPixelToCoords(pos);

                // 先处理目标选择按钮
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

                // 检查手牌点击
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

                // 点击牌库（抽牌）
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

        // 绘制牌库
        window.draw(deckShape);
        window.draw(deckLabel);

        // 显示牌库剩余牌数
        size_t remaining = game.deck.size();
        sf::Text remainingText(font, makeSfUtf8String(std::string(u8"剩余") + std::to_string(remaining) + std::string(u8"张")), 16);
        remainingText.setFillColor(sf::Color::White);
        auto remainingBounds = remainingText.getLocalBounds();
        remainingText.setOrigin({remainingBounds.position.x + remainingBounds.size.x / 2.f, remainingBounds.position.y + remainingBounds.size.y / 2.f});
        remainingText.setPosition({deckShape.getPosition().x + deckShape.getSize().x / 2.f, deckShape.getPosition().y + deckShape.getSize().y / 2.f - 24.f});
        window.draw(remainingText);

        // 计算爆炸猫概率并绘制条形指示器（条在牌库右侧）
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
        // 渐变颜色：按概率加深红色（简单方式）
        std::uint8_t red = static_cast<std::uint8_t>(200 + 55 * prob);
        barFill.setFillColor(sf::Color(red, 40, 40));
        window.draw(barFill);

        // 概率文本
        int pct = static_cast<int>(prob * 100.0f + 0.5f);
        sf::Text probText(font, makeSfUtf8String(std::string(u8"爆炸概率: ") + std::to_string(pct) + std::string(u8"%")), 14);
        probText.setFillColor(sf::Color::White);
        probText.setPosition({barPos.x, barPos.y - 18.f});
        window.draw(probText);

        // 绘制手牌
        updateHandDisplay();
        for (auto &shape : handShapes)
            window.draw(shape);
        for (auto &sprite : handSprites)
            window.draw(sprite);
        for (auto &text : handLabels)
            window.draw(text);

        // 绘制目标选择按钮
        updateTargetDisplay();
        for (auto &btn : targetButtons)
            window.draw(btn);
        for (auto &label : targetLabels)
            window.draw(label);

        // 信息文本
        if (!game.message.empty())
            messageText.setString(makeSfUtf8String(game.message));
        else
            messageText.setString(sf::String());
        window.draw(messageText);

        // 当前玩家
        currentPlayerText.setString(makeSfUtf8String(std::string(u8"\u5f53\u524d\uff1a") + game.getCurrentPlayer()->name +
                                                     (game.getCurrentPlayer()->alive ? "" : std::string(u8" (\u5df2\u6dd8\u6c70)"))));
        window.draw(currentPlayerText);

        // 抽牌倒计时（仅在需要摸牌时显示）
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
            game.update(); // 处理摸牌后的暂停
            if (game.gameOver)
            {
                Player *winner = game.getWinner();
                std::string winnerName = winner ? winner->name : std::string(u8"\u672a\u77e5");
                messageText.setString(makeSfUtf8String(std::string(u8"\u6e38\u620f\u7ed3\u675f\uff01\u80dc\u5229\u8005\uff1a") + winnerName));
                window.clear();
                window.draw(messageText);
                window.display();
                sf::sleep(sf::seconds(3));
                window.close();
                break;
            }
            render();
        }
    }
};

int main()
{
// 设置控制台编码支持
#ifdef _WIN32
    system("chcp 65001 > nul"); // 设置Windows控制台为UTF-8
#endif

    std::setlocale(LC_ALL, ".UTF-8"); // 设置locale为UTF-8

    int numPlayers;
    std::cout << u8"\u8bf7\u8f93\u5165\u73a9\u5bb6\u4eba\u6570" << " (2-6): ";
    std::cin >> numPlayers;
    if (numPlayers < 2 || numPlayers > 6)
    {
        std::cout << u8"\u4eba\u6570\u9519\u8bef\uff0c\u9ed8\u8ba4\u8bbe\u7f6e\u4e3a4\u4eba" << std::endl;
        numPlayers = 4;
    }

    Game game(numPlayers);
    UIManager ui(game);
    ui.run();

    return 0;
}
