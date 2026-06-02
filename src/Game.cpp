#include "Game.h"
#include <filesystem>
#include <fstream>
#include <algorithm>

namespace
{
    std::filesystem::path findDataRoot()
    {
        const std::filesystem::path candidates[] = {
            std::filesystem::current_path(),
            std::filesystem::current_path().parent_path(),
            std::filesystem::current_path().parent_path().parent_path(),
            std::filesystem::path(__FILE__).parent_path().parent_path()};

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

TetrisGame::TetrisGame(int requestedPlayers)
    : window(), player1(), player2(), isGameOver(false), isGameQuit(false), resultPhase(ResultPhase::Playing),
      resultLoserPlayer(0), resultWinnerPlayer(0), resultClock(), uiFont(), hasUiFont(false), tBackground(),
      tTiles(), emptyTexture(), sBackground(emptyTexture), window_width(1350), window_height(1000), imgBGno(1),
      imgSkinNo(1), requestedPlayerCount(std::clamp(requestedPlayers, 2, 6)), playerModeHint(), gameClock()
{
    const std::string title = "Dual Tetris - Selected players: " + std::to_string(requestedPlayerCount);
    window.create(sf::VideoMode({static_cast<unsigned int>(window_width), static_cast<unsigned int>(window_height)}), title);
    window.setKeyRepeatEnabled(false);

    if (requestedPlayerCount != 2)
        playerModeHint = "Current scene is 2-player battle. Selected " + std::to_string(requestedPlayerCount) + " players from start screen.";

    gameInitial();
}

TetrisGame::~TetrisGame() {}

void TetrisGame::LoadMediaData()
{
    const std::filesystem::path dataRoot = findDataRoot();

    // 尝试加载背景纹理（失败则生成白色）
    if (!dataRoot.empty())
    {
        std::filesystem::path bgFile = dataRoot / "bg" / (std::string("bg") + std::to_string(imgBGno) + ".jpg");
        if (!loadTextureFromCandidates(tBackground, {bgFile}))
        {
            // 背景缺失时保持白底
            sf::Image bgImage({1u, 1u}, sf::Color::White);
            tBackground = sf::Texture(bgImage);
        }
    }
    else
    {
        sf::Image bgImage({1u, 1u}, sf::Color::White);
        tBackground = sf::Texture(bgImage);
    }
    // 每种方块按图片名加载；支持 data/pai/mapping.txt 映射（格式：index filename.png）
    namespace fs = std::filesystem;
    const std::vector<fs::path> paiDirCandidates = dataRoot.empty()
                                                       ? std::vector<fs::path>{fs::path("data/pai"), fs::path("../data/pai"), fs::path("../../data/pai")}
                                                       : std::vector<fs::path>{dataRoot / "pai"};
    std::array<bool, 8> loaded{};
    loaded.fill(false);

    fs::path paiDir;
    for (const auto &candidate : paiDirCandidates)
    {
        if (fs::exists(candidate) && fs::is_directory(candidate))
        {
            paiDir = candidate;
            break;
        }
    }
    if (paiDir.empty())
        paiDir = paiDirCandidates.front();

    // 先尝试读取 mapping.txt
    fs::path mappingPath = fs::path(paiDir) / "mapping.txt";
    if (fs::exists(mappingPath))
    {
        std::ifstream in(mappingPath);
        std::string line;
        while (std::getline(in, line))
        {
            // 去掉注释与空行
            if (line.empty() || line[0] == '#')
                continue;
            std::istringstream iss(line);
            int idx = 0;
            std::string fname;
            if (!(iss >> idx >> fname))
                continue;
            if (idx < 1 || idx > 7)
                continue;
            fs::path filePath = fs::path(paiDir) / fname;
            if (loadTextureFromCandidates(tTiles[static_cast<size_t>(idx)], makeCandidatePaths(filePath)))
            {
                loaded[static_cast<size_t>(idx)] = true;
            }
        }
    }

    // 扫描目录内的 png 文件，按文件名顺序自动分配给未映射的索引
    std::vector<fs::path> pngs;
    if (fs::exists(paiDir))
    {
        for (auto &p : fs::directory_iterator(paiDir))
        {
            if (p.path().extension() == ".png")
                pngs.push_back(p.path());
        }
        std::sort(pngs.begin(), pngs.end());
    }
    size_t nextIdx = 1;
    for (auto &p : pngs)
    {
        // 找下一个未使用的索引
        while (nextIdx <= 7 && loaded[nextIdx])
            ++nextIdx;
        if (nextIdx > 7)
            break;
        if (loadTextureFromCandidates(tTiles[static_cast<size_t>(nextIdx)], makeCandidatePaths(p)))
        {
            loaded[nextIdx] = true;
            ++nextIdx;
        }
    }

    // 缺失的索引用占位颜色填充（7 种颜色顺序固定）
    const std::array<sf::Color, 7> fallbackColors = {
        sf::Color::Cyan,
        sf::Color::Yellow,
        sf::Color::Magenta,
        sf::Color::Green,
        sf::Color::Red,
        sf::Color::Blue,
        sf::Color::White};
    for (int i = 1; i <= 7; ++i)
    {
        if (!loaded[static_cast<size_t>(i)])
        {
            sf::Image tileImage({static_cast<unsigned int>(GRIDSIZE), static_cast<unsigned int>(GRIDSIZE)},
                                fallbackColors[static_cast<size_t>(i - 1)]);
            tTiles[static_cast<size_t>(i)] = sf::Texture(tileImage);
            loaded[static_cast<size_t>(i)] = true;
        }
    }
    sBackground.setTexture(tBackground, true);

    hasUiFont = uiFont.openFromFile("C:/Windows/Fonts/msyh.ttc");
    if (!hasUiFont)
        hasUiFont = uiFont.openFromFile("C:/Windows/Fonts/simhei.ttf");
    if (!hasUiFont)
        hasUiFont = uiFont.openFromFile("C:/Windows/Fonts/arial.ttf");
}

void TetrisGame::gameInitial()
{
    window.setFramerateLimit(60);
    LoadMediaData();
    player1.Initial(&tTiles, &window, rolePLAYER1);
    player2.Initial(&tTiles, &window, rolePLAYER2);
    isGameOver = false;
    resultPhase = ResultPhase::Playing;
    resultLoserPlayer = 0;
    resultWinnerPlayer = 0;
    gameClock.restart();
    resultClock.restart();
}

void TetrisGame::gameInput()
{
    while (const std::optional event = window.pollEvent())
    {
        if (event->is<sf::Event::Closed>())
        {
            window.close();
            isGameQuit = true;
        }
        if (resultPhase == ResultPhase::Playing)
        {
            player1.Input(event);
            player2.Input(event);
        }
    }
}

void TetrisGame::startResultSequence(int loserPlayer, int winnerPlayer)
{
    resultLoserPlayer = loserPlayer;
    resultWinnerPlayer = winnerPlayer;
    resultPhase = ResultPhase::EliminationNotice;
    resultClock.restart();
}

void TetrisGame::gameLogic()
{
    if (resultPhase == ResultPhase::Playing)
    {
        float time = gameClock.restart().asSeconds();
        if (time > 0.033f)
            time = 0.033f;
        player1.timer += time;
        player2.timer += time;
        player1.Logic();
        player2.Logic();

        if (player1.isGameOver || player2.isGameOver)
        {
            if (player1.isGameOver && !player2.isGameOver)
            {
                startResultSequence(1, 2);
            }
            else if (player2.isGameOver && !player1.isGameOver)
            {
                startResultSequence(2, 1);
            }
            else
            {
                resultLoserPlayer = 0;
                resultWinnerPlayer = 0;
                resultPhase = ResultPhase::VictoryNotice;
                resultClock.restart();
                isGameOver = true;
            }
        }
        return;
    }

    float elapsed = resultClock.getElapsedTime().asSeconds();
    if (resultPhase == ResultPhase::EliminationNotice && elapsed >= 1.5f)
    {
        resultPhase = ResultPhase::VictoryNotice;
        resultClock.restart();
    }
    else if (resultPhase == ResultPhase::VictoryNotice && elapsed >= 1.5f)
    {
        isGameOver = true;
    }
}

void TetrisGame::gameDraw()
{
    window.clear(sf::Color::Black);
    window.draw(sBackground);
    // 绘制玩家边框，帮助定位舞台边界
    {
        sf::RectangleShape rect1(sf::Vector2f(static_cast<float>(STAGE_WIDTH * GRIDSIZE), static_cast<float>(STAGE_HEIGHT * GRIDSIZE)));
        rect1.setPosition(sf::Vector2f(static_cast<float>(player1.getCorner().x), static_cast<float>(player1.getCorner().y)));
        rect1.setFillColor(sf::Color::Transparent);
        rect1.setOutlineColor(sf::Color::White);
        rect1.setOutlineThickness(2.0f);
        window.draw(rect1);

        sf::RectangleShape rect2(rect1);
        rect2.setPosition(sf::Vector2f(static_cast<float>(player2.getCorner().x), static_cast<float>(player2.getCorner().y)));
        rect2.setOutlineColor(sf::Color::Cyan);
        window.draw(rect2);
    }
    player1.Draw();
    player2.Draw();

    drawModeHint();

    if (resultPhase != ResultPhase::Playing)
        drawResultOverlay();

    window.display();
}

void TetrisGame::drawModeHint()
{
    if (!hasUiFont || playerModeHint.empty())
        return;

    sf::RectangleShape hintBg(sf::Vector2f(1200.f, 40.f));
    hintBg.setPosition(sf::Vector2f(75.f, 30.f));
    hintBg.setFillColor(sf::Color(0, 0, 0, 120));
    window.draw(hintBg);

    sf::Text hint(uiFont);
    hint.setCharacterSize(24);
    hint.setFillColor(sf::Color(245, 230, 140));
    hint.setString(playerModeHint);
    hint.setPosition(sf::Vector2f(90.f, 35.f));
    window.draw(hint);
}

void TetrisGame::drawResultOverlay()
{
    if (!hasUiFont)
        return;

    sf::RectangleShape overlay(sf::Vector2f(static_cast<float>(window_width), static_cast<float>(window_height)));
    overlay.setFillColor(sf::Color(0, 0, 0, 165));
    window.draw(overlay);

    sf::Text title(uiFont);
    title.setCharacterSize(54);
    title.setStyle(sf::Text::Bold);
    title.setFillColor(sf::Color::White);

    sf::Text detail(uiFont);
    detail.setCharacterSize(32);
    detail.setFillColor(sf::Color(230, 230, 230));

    if (resultPhase == ResultPhase::EliminationNotice)
    {
        title.setString(resultLoserPlayer == 1 ? "Player 1 eliminated" : "Player 2 eliminated");
        detail.setString("Bomb card triggered, player removed from the match");
    }
    else if (resultPhase == ResultPhase::VictoryNotice)
    {
        if (resultWinnerPlayer == 0)
        {
            title.setString("Draw");
            detail.setString("Both players were eliminated");
        }
        else
        {
            title.setString(resultWinnerPlayer == 1 ? "Player 1 wins" : "Player 2 wins");
            detail.setString("The other player has been eliminated");
        }
    }
    else
    {
        title.setString("Match over");
        detail.setString("No winner could be determined");
    }

    auto titleBounds = title.getLocalBounds();
    title.setOrigin(titleBounds.getCenter());
    title.setPosition(sf::Vector2f(static_cast<float>(window_width) * 0.5f, static_cast<float>(window_height) * 0.42f));

    auto detailBounds = detail.getLocalBounds();
    detail.setOrigin(detailBounds.getCenter());
    detail.setPosition(sf::Vector2f(static_cast<float>(window_width) * 0.5f, static_cast<float>(window_height) * 0.52f));

    window.draw(title);
    window.draw(detail);
}

void TetrisGame::gameRun()
{
    while (window.isOpen() && !isGameQuit && !isGameOver)
    {
        gameInput();
        gameLogic();
        gameDraw();
    }
    // 游戏结束后等待2秒自动关闭
    sf::sleep(sf::seconds(2));
}