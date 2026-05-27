#include "Tetris.h"
#include <cmath>

Tetris::Tetris()
    : isGameOver(false), timer(0.0f), delay(0.3f), score(0), role(roleNONE),
      colorNum(1), nextColorNum(1), nextShapeIndex(0), dx(0), isRotate(false), tTiles(nullptr), window(nullptr),
      sTiles(emptyTexture)
{
    srand(static_cast<unsigned>(time(nullptr)));
    initFigures();
    // 初始化场地
    for (int i = 0; i < FIELD_HEIGHT; ++i)
        for (int j = 0; j < FIELD_WIDTH; ++j)
            Field[i][j] = 0;
}

Tetris::~Tetris() {}

void Tetris::initFigures()
{
    // 编码方式：每个数字0~7对应2x4矩阵中的一个格子
    // 行优先：0 1 2 3 在第一行，4 5 6 7 在第二行
    int raw[7][4] = {
        {0, 2, 4, 6}, // I
        {1, 3, 4, 6}, // S
        {2, 3, 4, 5}, // Z
        {2, 3, 4, 6}, // T
        {1, 2, 4, 6}, // L
        {2, 4, 5, 6}, // J
        {1, 2, 3, 4}  // O
    };
    for (int i = 0; i < 7; ++i)
        for (int j = 0; j < 4; ++j)
            Figures[i][j] = raw[i][j];
}

void Tetris::Initial(const std::array<sf::Texture, 8> *tex, sf::RenderWindow *w, PLAYROLE r)
{
    tTiles = tex;
    window = w;
    role = r;
    if (role == rolePLAYER1)
        mCornPoint = {156, 174}; // 根据屏幕布局调整
    else if (role == rolePLAYER2)
        mCornPoint = {844, 174};

    sTiles.setTexture((*tTiles)[1], true);
    sTiles.setScale({1.0f, 1.0f});

    // 重置场地
    for (int i = 0; i < FIELD_HEIGHT; ++i)
        for (int j = 0; j < FIELD_WIDTH; ++j)
            Field[i][j] = 0;

    // 生成第一个方块
    nextShapeIndex = rand() % 7;
    nextColorNum = nextShapeIndex + 1;
    spawnNewPiece();
    isGameOver = false;
    timer = 0.0f;
    delay = 0.3f;
    dx = 0;
    isRotate = false;
    score = 0;
}

void Tetris::spawnNewPiece()
{
    // 将下一个方块作为当前，再随机生成下一个
    colorNum = nextColorNum;
    int shape = nextShapeIndex;
    nextShapeIndex = rand() % 7;
    nextColorNum = nextShapeIndex + 1;

    // 初始化当前方块的坐标（顶部居中）
    for (int i = 0; i < 4; ++i)
    {
        int code = Figures[shape][i];
        currentSquare[i].x = code % 2; // 列 x = code % 2 (0或1)
        currentSquare[i].y = code / 2; // 行 y = code / 2 (0~3)
        // 将形状水平偏移到舞台中央 (FIELD_WIDTH/2 - 1)
        currentSquare[i].x += (FIELD_WIDTH / 2 - 1);
    }
    // 检查生成时是否立即碰撞 -> 游戏结束
    if (!hitTest())
    {
        isGameOver = true;
    }
}

void Tetris::fixCurrentPiece()
{
    // 将当前方块固定到Field
    for (int i = 0; i < 4; ++i)
    {
        int x = currentSquare[i].x;
        int y = currentSquare[i].y;
        if (y >= 0 && y < FIELD_HEIGHT && x >= 0 && x < FIELD_WIDTH)
            Field[y][x] = colorNum;
    }
    // 消除满行并得分
    clearLines();
    // 生成新方块
    spawnNewPiece();
}

void Tetris::clearLines()
{
    int linesCleared = 0;
    for (int y = FIELD_HEIGHT - 1; y >= 0;)
    {
        bool full = true;
        for (int x = 0; x < FIELD_WIDTH; ++x)
        {
            if (Field[y][x] == 0)
            {
                full = false;
                break;
            }
        }
        if (full)
        {
            // 消除该行：将上方所有行下移
            for (int yy = y; yy > 0; --yy)
            {
                for (int x = 0; x < FIELD_WIDTH; ++x)
                {
                    Field[yy][x] = Field[yy - 1][x];
                }
            }
            // 顶部行清零
            for (int x = 0; x < FIELD_WIDTH; ++x)
                Field[0][x] = 0;
            linesCleared++;
            // 继续检查同一y位置（新移下来的行）
        }
        else
        {
            y--;
        }
    }
    // 计分
    if (linesCleared > 0)
    {
        score += linesCleared * 100;
    }
}

bool Tetris::hitTest()
{
    for (int i = 0; i < 4; ++i)
    {
        int x = currentSquare[i].x;
        int y = currentSquare[i].y;
        if (x < 0 || x >= FIELD_WIDTH || y >= STAGE_HEIGHT)
            return false;
        if (y >= 0 && Field[y][x] != 0)
            return false;
    }
    return true;
}

void Tetris::Input(const std::optional<sf::Event> &event)
{
    if (isGameOver)
        return;
    if (!event.has_value())
        return;

    if (role == rolePLAYER1)
    {
        if (event->is<sf::Event::KeyPressed>())
        {
            if (const auto *keyEvent = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyEvent->code == sf::Keyboard::Key::W)
                    isRotate = true;
                else if (keyEvent->code == sf::Keyboard::Key::A)
                    dx = -1;
                else if (keyEvent->code == sf::Keyboard::Key::D)
                    dx = 1;
                else if (keyEvent->code == sf::Keyboard::Key::S)
                    delay = 0.05f;
            }
        }
        if (event->is<sf::Event::KeyReleased>())
        {
            if (const auto *keyEvent = event->getIf<sf::Event::KeyReleased>())
            {
                if (keyEvent->code == sf::Keyboard::Key::A || keyEvent->code == sf::Keyboard::Key::D)
                    dx = 0;
                if (keyEvent->code == sf::Keyboard::Key::S)
                    delay = 0.3f;
            }
        }
    }
    else if (role == rolePLAYER2)
    {
        if (event->is<sf::Event::KeyPressed>())
        {
            if (const auto *keyEvent = event->getIf<sf::Event::KeyPressed>())
            {
                if (keyEvent->code == sf::Keyboard::Key::Up)
                    isRotate = true;
                else if (keyEvent->code == sf::Keyboard::Key::Left)
                    dx = -1;
                else if (keyEvent->code == sf::Keyboard::Key::Right)
                    dx = 1;
                else if (keyEvent->code == sf::Keyboard::Key::Down)
                    delay = 0.05f;
            }
        }
        if (event->is<sf::Event::KeyReleased>())
        {
            if (const auto *keyEvent = event->getIf<sf::Event::KeyReleased>())
            {
                if (keyEvent->code == sf::Keyboard::Key::Left || keyEvent->code == sf::Keyboard::Key::Right)
                    dx = 0;
                if (keyEvent->code == sf::Keyboard::Key::Down)
                    delay = 0.3f;
            }
        }
    }
}

void Tetris::Logic()
{
    if (isGameOver)
        return;

    // 水平移动
    for (int i = 0; i < 4; ++i)
    {
        tempSquare[i] = currentSquare[i];
        currentSquare[i].x += dx;
    }
    if (!hitTest())
    {
        for (int i = 0; i < 4; ++i)
            currentSquare[i] = tempSquare[i];
    }
    dx = 0;

    // 旋转
    if (isRotate)
    {
        sf::Vector2i p = currentSquare[1];
        for (int i = 0; i < 4; ++i)
            tempSquare[i] = currentSquare[i];
        for (int i = 0; i < 4; ++i)
        {
            int x = currentSquare[i].y - p.y;
            int y = currentSquare[i].x - p.x;
            currentSquare[i].x = p.x - x;
            currentSquare[i].y = p.y + y;
        }
        if (!hitTest())
        {
            for (int i = 0; i < 4; ++i)
                currentSquare[i] = tempSquare[i];
        }
        isRotate = false;
    }

    // 下落计时
    if (timer > delay)
    {
        for (int i = 0; i < 4; ++i)
        {
            tempSquare[i] = currentSquare[i];
            currentSquare[i].y += 1;
        }
        if (!hitTest())
        {
            for (int i = 0; i < 4; ++i)
                currentSquare[i] = tempSquare[i];
            fixCurrentPiece();
        }
        timer = 0.0f;
    }
}

void Tetris::addGarbageRow(int lines)
{
    for (int l = 0; l < lines; ++l)
    {
        // 将现有场上所有行向上移动一行（底部留空）
        for (int y = 0; y < FIELD_HEIGHT - 1; ++y)
        {
            for (int x = 0; x < FIELD_WIDTH; ++x)
            {
                Field[y][x] = Field[y + 1][x];
            }
        }
        // 在最底部生成一行垃圾（随机一个缺口）
        int missing = rand() % FIELD_WIDTH;
        for (int x = 0; x < FIELD_WIDTH; ++x)
        {
            Field[FIELD_HEIGHT - 1][x] = (x == missing) ? 0 : 1;
        }
        // 如果顶部被推到超出界限，游戏结束
        for (int x = 0; x < FIELD_WIDTH; ++x)
        {
            if (Field[0][x] != 0)
            {
                isGameOver = true;
                break;
            }
        }
    }
}

void Tetris::Draw()
{
    if (!window || !tTiles)
        return;
    // 绘制固定方块（只绘制可见区域 STAGE_HEIGHT）
    for (int i = 0; i < STAGE_HEIGHT; ++i)
    {
        for (int j = 0; j < STAGE_WIDTH; ++j)
        {
            int val = Field[i][j];
            if (val == 0)
                continue;
            const sf::Texture *tileTexture = &(*tTiles)[static_cast<size_t>(val)];
            sTiles.setTexture(*tileTexture, true);
            const auto textureSize = tileTexture->getSize();
            sTiles.setScale(sf::Vector2f(static_cast<float>(GRIDSIZE) / static_cast<float>(textureSize.x),
                                         static_cast<float>(GRIDSIZE) / static_cast<float>(textureSize.y)));
            sTiles.setPosition(sf::Vector2f(j * GRIDSIZE, i * GRIDSIZE));
            sTiles.move(sf::Vector2f(mCornPoint.x, mCornPoint.y));
            window->draw(sTiles);
        }
    }
    // 绘制活动方块
    for (int i = 0; i < 4; ++i)
    {
        int x = currentSquare[i].x;
        int y = currentSquare[i].y;
        if (y >= 0 && y < STAGE_HEIGHT)
        {
            const sf::Texture *tileTexture = &(*tTiles)[static_cast<size_t>(colorNum)];
            sTiles.setTexture(*tileTexture, true);
            const auto textureSize = tileTexture->getSize();
            sTiles.setScale(sf::Vector2f(static_cast<float>(GRIDSIZE) / static_cast<float>(textureSize.x),
                                         static_cast<float>(GRIDSIZE) / static_cast<float>(textureSize.y)));
            sTiles.setPosition(sf::Vector2f(x * GRIDSIZE, y * GRIDSIZE));
            sTiles.move(sf::Vector2f(mCornPoint.x, mCornPoint.y));
            window->draw(sTiles);
        }
    }
}

sf::Vector2i Tetris::getCorner() const
{
    return mCornPoint;
}