#pragma once
#include "Tipos.hpp"
#include "Reglas.hpp"
#include <SFML/Graphics.hpp>
#include <map>
#include <vector>
#include <string>

// ---------------------- Centrar y escalar sprite ----------------------
inline void centrarYescalar(sf::Sprite &s, int fila, int col){
    sf::FloatRect b = s.getLocalBounds();
    s.setOrigin(b.width/2.f, b.height/2.f);
    sf::Vector2f centro = centroCasilla(fila,col); // proviene de reglas.hpp
    s.setPosition(centro);
}

// ---------------------- Cargar texturas ----------------------
inline bool cargarTxt(sf::Texture &t, const std::string &ruta){
    if(!t.loadFromFile(ruta)){
        std::cerr << "No se pudo cargar: " << ruta << "\n";
        return false;
    }
    return true;
}

// ---------------------- Inicializar tablero gráfico ----------------------
inline void inicializarTablero(sf::Sprite &fondo, sf::Sprite &tablero, std::map<std::string,sf::Texture> &tex){
    fondo.setTexture(tex["Fondo"]);
    tablero.setTexture(tex["Tablero"]);
    tablero.setPosition((float)TABLERO_X, (float)TABLERO_Y);
    float escalaTab = (8.0f * TAM_CASILLA) / (float)tex["Tablero"].getSize().x;
    tablero.setScale(escalaTab, escalaTab);
}

// ---------------------- Añadir pieza ----------------------
inline void addPieza(std::vector<Pieza> &piezas, std::map<std::string,sf::Texture> &tex,
                     TipoPieza tipo, ColorPieza color, int fila, int col,
                     const std::string &texKey, const std::string &baseKey, int &contadorId){
    Pieza p;
    p.id = baseKey + "_" + std::to_string(contadorId++);
    p.tipo = tipo;
    p.color = color;
    p.fila = fila;
    p.col = col;
    p.sprite.setTexture(tex[texKey]);

    const sf::Texture* t = p.sprite.getTexture();
    if (t){
        p.baseSx = (float)TAM_CASILLA / (float)t->getSize().x;
        p.baseSy = (float)TAM_CASILLA / (float)t->getSize().y;
        p.sprite.setScale(p.baseSx, p.baseSy);
    }

    centrarYescalar(p.sprite, fila, col);
    tableroLogico[fila][col] = (int)piezas.size();
    piezas.push_back(std::move(p));
}

// ---------------------- Dibujar movimientos válidos ----------------------
inline void dibujarMovimientos(sf::RenderWindow &window, const std::vector<std::pair<int,int>> &movimientosValidos){
    sf::CircleShape dot((float)TAM_CASILLA * 0.12f);
    dot.setOrigin(dot.getRadius(), dot.getRadius());
    dot.setFillColor(DOT_COLOR);
    for (auto &m : movimientosValidos){
        sf::Vector2f c = centroCasilla(m.first, m.second); // proviene de reglas.hpp
        dot.setPosition(c);
        window.draw(dot);
    }
}

// ---------------------- Dibujar jaque ----------------------
inline void dibujarJaque(sf::RenderWindow &window, const std::vector<Pieza> &piezas, bool blancoEnJaque, bool negroEnJaque){
    if (blancoEnJaque || negroEnJaque){
        int idxRey = -1;
        ColorPieza c = blancoEnJaque ? ColorPieza::White : ColorPieza::Black;
        idxRey = encontrarIndiceRey(piezas, c); // proviene de reglas.hpp
        if (idxRey != -1 && piezas[idxRey].alive){
            sf::RectangleShape r(sf::Vector2f((float)TAM_CASILLA, (float)TAM_CASILLA));
            r.setFillColor(sf::Color::Transparent);
            r.setOutlineColor(sf::Color::Red);
            r.setOutlineThickness(3.0f);
            float left = TABLERO_X + piezas[idxRey].col * TAM_CASILLA;
            float top  = TABLERO_Y + piezas[idxRey].fila * TAM_CASILLA;
            r.setPosition(left, top);
            window.draw(r);
        }
    }
}

// ---------------------- Dibujar piezas ----------------------
inline void dibujarPiezas(sf::RenderWindow &window, const std::vector<Pieza> &piezas, int idxSeleccionado){
    for (int i=0;i<(int)piezas.size();++i){
        if (i == idxSeleccionado) continue;
        if (piezas[i].alive || piezas[i].animandoCaptura) window.draw(piezas[i].sprite);
    }
}

// ---------------------- Dibujar sombra y pieza arrastrada ----------------------
inline void dibujarArrastrada(sf::RenderWindow &window, const Pieza &pieza, sf::CircleShape &sombra){
    sf::Vector2f pos = pieza.sprite.getPosition();
    sombra.setPosition(pos.x + 6.f, pos.y + 10.f); // ahora sombra no es const
    window.draw(sombra);
    window.draw(pieza.sprite);
}