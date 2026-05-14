#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <algorithm>
#include <chrono>
#include <clocale>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

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
    int forcedTargetIndex = -1; // 当前被强制代摸的玩家索引（用于 Attack）
    int returnToPlayer = -1;    // Attack 后应回到的玩家索引
    bool returnPending = false; // 是否在等待将回合交还给 returnToPlayer
    PendingAction pendingAction = PendingAction::None;
    std::string message;
    int drawPauseFrames = 0;
    sf::Clock turnClock;
    const float turnTimeLimitSeconds = 30.f;

    Game(int numPlayers)
        : bonusTurns(numPlayers, 0)
    {
        for (int i = 1; i <= numPlayers; ++i)
            players.emplace_back(std::string(u8"\u73a9\u5bb6") + std::to_string(i));

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

    void nextTurn()
    {
        if (gameOver)
            return;

        // 如果之前有一个被强制代摸的流程正在进行，且当前刚完成的是被代摸的玩家
        if (returnPending && forcedTargetIndex >= 0 && currentPlayer == forcedTargetIndex)
        {
            // 将回合交还给原先的下一位玩家（returnToPlayer）
            currentPlayer = returnToPlayer;
            returnPending = false;
            forcedTargetIndex = -1;
            returnToPlayer = -1;
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
        return !gameOver && drawPauseFrames == 0 && drawRequired && !isWaitingForTarget() && players[currentPlayer].alive;
    }

    int getRemainingDrawSeconds() const
    {
        if (!canDrawNow())
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
                // 计算原始顺序下的下一个玩家（回合结束后应回到此人）
                int step = direction ? 1 : -1;
                int n = static_cast<int>(players.size());
                int nextAfterCurrent = (currentPlayer + step + n) % n;

                forcedTargetIndex = targetIndex;
                forcedNextPlayer = targetIndex;    // 立即把目标设为下个当前玩家去代摸
                returnToPlayer = nextAfterCurrent; // 代摸完成后应回到此玩家
                returnPending = true;
                drawRequired = false;
                nextTurn();
                if (!gameOver)
                {
                    message = players[attacker].name + std::string(u8" \u7529\u9505\u7ed9 ") + players[targetIndex].name + std::string(u8"\uff0c\u73b0\u5728\u8f6e\u5230 ") + players[currentPlayer].name;
                }
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
                message = actor.name + std::string(u8" \u4ece ") + victim.name + std::string(u8" \u7d22\u8981\u5230 ") + std::string(u8"“") + cardName(stolen.type) + std::string(u8"”");
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
        if (drawPauseFrames > 0)
        {
            drawPauseFrames--;
            if (drawPauseFrames == 0)
                nextTurn();
            return;
        }

        if (canDrawNow() && turnClock.getElapsedTime().asSeconds() >= turnTimeLimitSeconds)
        {
            message = getCurrentPlayer()->name + std::string(u8" \u8d85\u65f6\uff0c\u7cfb\u7edf\u5f3a\u5236\u4ece\u724c\u9876\u62bd\u724c");
            mustDrawFromBottom = false;
            handleDrawnCard(deck.drawTop());
        }
    }

    void endPlayPhase()
    {
        if (!canDrawNow())
            return;

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
            }
        }
        else
        {
            player.hand.push_back(c);
            message = player.name + std::string(u8" \u62bd\u5230\u4e86 ") + cardName(c.type);
        }

        drawPauseFrames = 6000;
    }

    void playCard(int handIndex)
    {
        if (gameOver)
            return;

        Player &p = *getCurrentPlayer();
        if (handIndex < 0 || handIndex >= static_cast<int>(p.hand.size()))
            return;

        Card card = p.hand[handIndex];

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
            drawRequired = false;
            nextTurn();
            break;
        case CardType::Reverse:
            direction = !direction;
            drawRequired = false;
            nextTurn();
            break;
        case CardType::Attack:
            pendingAction = PendingAction::Attack;
            message = u8"\u8bf7\u9009\u62e9\u7529\u9505\u76ee\u6807\uff08\u53ef\u9009\u81ea\u5df1\uff09";
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

    sf::RectangleShape deckShape;
    sf::Text deckLabel; // SFML 3 要求 Text 构造时带字体

    std::vector<sf::RectangleShape> handShapes;
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
          deckLabel(font), // ✅ 构造时传入字体
          messageText(font),
          currentPlayerText(font),
          timerText(font)
    {
        // ✅ 使用系统自带中文字体（支持中文）
        // 尝试多个字体路径，优先级：微软雅黑 > 宋体 > 黑体
        bool fontLoaded = false;

        // 打开日志文件
        std::ofstream logFile("font_debug.log");
        logFile << "=== 字体加载调试 ===" << std::endl;

        // 使用绝对路径
        std::vector<std::string> fontPaths = {
            "C:\\Windows\\Fonts\\msyh.ttc",   // 微软雅黑
            "C:\\Windows\\Fonts\\simhei.ttf", // 黑体
            "C:\\Windows\\Fonts\\simsun.ttc"  // 宋体
        };

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
            else
            {
                logFile << "✗ 失败: " << path << std::endl;
            }
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
        deckLabel.setPosition({570, 350});

        messageText = sf::Text(font, sf::String(), 24);
        messageText.setFillColor(sf::Color::White);
        messageText.setPosition({50, 50});

        currentPlayerText = sf::Text(font, sf::String(), 30);
        currentPlayerText.setFillColor(sf::Color::Yellow);
        currentPlayerText.setPosition({50, 100});

        timerText = sf::Text(font, sf::String(), 24);
        timerText.setFillColor(sf::Color(255, 215, 0));
        timerText.setPosition({50, 140});
    }

    void updateHandDisplay()
    {
        handShapes.clear();
        handLabels.clear();
        Player *player = game.getCurrentPlayer();
        if (!player || !player->alive)
            return;

        const float startX = 100.f;
        const float startY = 560.f;
        const float cardWidth = 80.f;
        const float cardHeight = 110.f;
        const float gap = 12.f;
        const int cardsPerRow = std::max(1, static_cast<int>((window.getSize().x - static_cast<unsigned int>(startX * 2) + static_cast<unsigned int>(gap)) / (cardWidth + gap)));

        for (size_t i = 0; i < player->hand.size(); ++i)
        {
            int row = static_cast<int>(i) / cardsPerRow;
            int column = static_cast<int>(i) % cardsPerRow;
            float x = startX + column * (cardWidth + gap);
            float y = startY + row * (cardHeight + 10.f);

            // 卡牌背景
            sf::RectangleShape rect({cardWidth, cardHeight});
            rect.setPosition({x, y});
            rect.setFillColor(cardColor(player->hand[i].type));
            handShapes.push_back(rect);

            // 卡牌文字（✅ 必须带字体构造）
            sf::Text label(font, makeSfUtf8String(cardName(player->hand[i].type)), 14);
            label.setFillColor(sf::Color::White);
            label.setPosition({x + 5.f, y + 40.f});
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

            sf::Text label(font, makeSfUtf8String(game.players[playerIndex].name), 18);
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
                // 转换为 sf::Vector2f 以调用 contains
                sf::Vector2f mousePos(static_cast<float>(pos.x), static_cast<float>(pos.y));

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

        // 绘制手牌
        updateHandDisplay();
        for (auto &shape : handShapes)
            window.draw(shape);
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
                messageText.setString(makeSfUtf8String(std::string(u8"\u6e38\u620f\u7ed3\u675f\uff01\u80dc\u5229\u8005\uff1a") + game.getCurrentPlayer()->name));
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