#include <SFML/Graphics.hpp>
#include <iostream>
#include <map>
#include <string>

// Tamaño de una casilla en píxeles
const int TILE_SIZE = 80;

// Función para cargar una textura con verificación
bool loadTexture(sf::Texture& texture, const std::string& filename) {
    if (!texture.loadFromFile(filename)) {
        std::cout << "Error cargando: " << filename << "\n";
        return false;
    }
    return true;
}

int main() {

    sf::RenderWindow window(sf::VideoMode(640, 640), "Ajedrez SFML");

    // ---------------------------
    // Cargar tablero
    // ---------------------------
    sf::Texture boardTexture;
    if (!loadTexture(boardTexture, "assets/images/board.png"))
        return -1;

    sf::Sprite boardSprite(boardTexture);

    // ---------------------------
    // Cargar texturas de piezas
    // ---------------------------
    std::map<std::string, sf::Texture> textures;

    std::string pieceNames[6] = {"pawn", "rook", "knight", "bishop", "queen", "king"};

    for (const std::string& name : pieceNames) {
        textures["white_" + name].loadFromFile("assets/images/pieces/white_" + name + ".png");
        textures["black_" + name].loadFromFile("assets/images/pieces/black_" + name + ".png");
    }

    // ---------------------------
    // Crear sprites de piezas
    // ---------------------------
    std::map<std::string, sf::Sprite> sprites;

    for (auto& t : textures) {
        sprites[t.first].setTexture(t.second);
        sprites[t.first].setScale(80.0f / t.second.getSize().x, 80.0f / t.second.getSize().y);
    }

    // ---------------------------
    // Posiciones iniciales en el tablero
    // ---------------------------

    // Función para posicionar una pieza
    auto setPos = [&](sf::Sprite& s, int col, int row) {
        s.setPosition(col * TILE_SIZE, row * TILE_SIZE);
    };

    // Peones
    for (int i = 0; i < 8; i++) {
        setPos(sprites["white_pawn"], i, 6);
        sprites["white_pawn" + std::to_string(i)] = sprites["white_pawn"];
        setPos(sprites["white_pawn" + std::to_string(i)], i, 6);

        setPos(sprites["black_pawn"], i, 1);
        sprites["black_pawn" + std::to_string(i)] = sprites["black_pawn"];
        setPos(sprites["black_pawn" + std::to_string(i)], i, 1);
    }

    // Torres
    sprites["white_rook0"] = sprites["white_rook"];
    sprites["white_rook1"] = sprites["white_rook"];
    setPos(sprites["white_rook0"], 0, 7);
    setPos(sprites["white_rook1"], 7, 7);

    sprites["black_rook0"] = sprites["black_rook"];
    sprites["black_rook1"] = sprites["black_rook"];
    setPos(sprites["black_rook0"], 0, 0);
    setPos(sprites["black_rook1"], 7, 0);

    // Caballos
    sprites["white_knight0"] = sprites["white_knight"];
    sprites["white_knight1"] = sprites["white_knight"];
    setPos(sprites["white_knight0"], 1, 7);
    setPos(sprites["white_knight1"], 6, 7);

    sprites["black_knight0"] = sprites["black_knight"];
    sprites["black_knight1"] = sprites["black_knight"];
    setPos(sprites["black_knight0"], 1, 0);
    setPos(sprites["black_knight1"], 6, 0);

    // Alfiles
    sprites["white_bishop0"] = sprites["white_bishop"];
    sprites["white_bishop1"] = sprites["white_bishop"];
    setPos(sprites["white_bishop0"], 2, 7);
    setPos(sprites["white_bishop1"], 5, 7);

    sprites["black_bishop0"] = sprites["black_bishop"];
    sprites["black_bishop1"] = sprites["black_bishop"];
    setPos(sprites["black_bishop0"], 2, 0);
    setPos(sprites["black_bishop1"], 5, 0);

    // Reina
    sprites["white_queen0"] = sprites["white_queen"];
    setPos(sprites["white_queen0"], 3, 7);

    sprites["black_queen0"] = sprites["black_queen"];
    setPos(sprites["black_queen0"], 3, 0);

    // Rey
    sprites["white_king0"] = sprites["white_king"];
    setPos(sprites["white_king0"], 4, 7);

    sprites["black_king0"] = sprites["black_king"];
    setPos(sprites["black_king0"], 4, 0);

    // ---------------------------
    // Bucle principal
    // ---------------------------
    while (window.isOpen()) {
        sf::Event event;

        while (window.pollEvent(event)) {
            if (event.type == sf::Event::Closed)
                window.close();
        }

        window.clear();

        // Dibujar tablero
        window.draw(boardSprite);

        // Dibujar piezas
        for (auto& s : sprites) {
            window.draw(s.second);
        }

        window.display();
    }

    return 0;
}
