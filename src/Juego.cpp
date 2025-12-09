// Juego.cpp
// Ajedrez SFML - Con jaque, detección de jaque mate, enroque normal y extendido (3 casillas),
// pieza "guardia" invulnerable por 1 turno, y promoción con UI.
// Requisitos: SFML (graphics/window/system). Imágenes en assets/images/

#include <SFML/Graphics.hpp>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cmath>
using namespace std;

// ---------------------- Configuración ----------------------
const int TAM_CASILLA = 70;
const int TABLERO_X   = 381; // coordenada X del tablero (sin márgenes)
const int TABLERO_Y   = 50;  // coordenada Y del tablero (sin márgenes)
const int FILAS = 8;
const int COLS  = 8;

// tolerancia (px) para aceptar un "drop" en la casilla más cercana
const float RADIO_ACEPTACION = 40.0f;

// animación captura
const float DURACION_ANIMACION_CAPTURA = 0.30f; // segundos

// dot color
const sf::Color DOT_COLOR(200, 200, 255, 220);

// ---------------------- Tipos ----------------------
enum class TipoPieza { Pawn, Rook, Knight, Bishop, Queen, King };
enum class ColorPieza { White, Black };

struct Pieza {
    string id;           // id único
    TipoPieza tipo;
    ColorPieza color;
    int fila, col;       // posición lógica (-1,-1 si fuera)
    sf::Sprite sprite;

    // para animaciones / restaurar escala
    float baseSx = 1.0f;
    float baseSy = 1.0f;

    bool alive = true;
    bool animandoCaptura = false;
    sf::Clock animClock;

    // si la pieza ya se movió (importante para enroque)
    bool hasMoved = false;

    // protección "guardia": si true, no puede ser capturada durante el turno de protección activo
    bool protegido = false;
};

// tablero lógico: índice de vector piezas, o -1 si vacío
int tableroLogico[FILAS][COLS];

// ---------------------- Helpers ----------------------
bool dentroTablero(int f,int c){ return f>=0 && f<FILAS && c>=0 && c<COLS; }

sf::Vector2f centroCasilla(int fila,int col){
    float x = TABLERO_X + col * TAM_CASILLA + TAM_CASILLA / 2.0f;
    float y = TABLERO_Y + fila * TAM_CASILLA + TAM_CASILLA / 2.0f;
    return {x,y};
}

pair<int,int> casillaMasCercana(float px, float py){
    float fx = (px - TABLERO_X) / (float)TAM_CASILLA;
    float fy = (py - TABLERO_Y) / (float)TAM_CASILLA;
    int c = (int)floor(fx + 0.5f);
    int r = (int)floor(fy + 0.5f);
    if (r<0) r=0; if (r>FILAS-1) r=FILAS-1;
    if (c<0) c=0; if (c>COLS-1) c=COLS-1;
    return {r,c};
}

bool cargarTxt(sf::Texture &t, const string &ruta){
    if(!t.loadFromFile(ruta)){
        cerr << "No se pudo cargar: " << ruta << "\n";
        return false;
    }
    return true;
}

// ---------------------- Movimiento / reglas ----------------------
bool lineaLibre(int f1,int c1,int f2,int c2){
    int dx = (c2>c1)?1: (c2<c1)? -1: 0;
    int dy = (f2>f1)?1: (f2<f1)? -1: 0;
    int x = c1 + dx;
    int y = f1 + dy;
    while(x != c2 || y != f2){
        if (!dentroTablero(y,x)) return false;
        if (tableroLogico[y][x] != -1) return false;
        x += dx; y += dy;
    }
    return true;
}

// Versión ligera para atacar (sin enroque)
bool puedeAtacar(const vector<Pieza>& piezas, int attIdx, int f, int c){
    if (attIdx < 0 || attIdx >= (int)piezas.size()) return false;
    const Pieza &p = piezas[attIdx];
    if (!p.alive) return false;
    int sF = p.fila, sC = p.col;
    if (!dentroTablero(sF,sC)) return false;
    int dx = c - sC;
    int dy = f - sF;
    int adx = abs(dx), ady = abs(dy);

    switch(p.tipo){
        case TipoPieza::Pawn: {
            int dir = (p.color == ColorPieza::White) ? -1 : 1;
            if (ady==1 && adx==1 && dy==dir) return true;
            return false;
        }
        case TipoPieza::Rook: {
            if (dx!=0 && dy!=0) return false;
            return lineaLibre(sF,sC,f,c);
        }
        case TipoPieza::Bishop: {
            if (adx!=ady) return false;
            return lineaLibre(sF,sC,f,c);
        }
        case TipoPieza::Queen: {
            if (!((dx==0) || (dy==0) || (adx==ady))) return false;
            return lineaLibre(sF,sC,f,c);
        }
        case TipoPieza::Knight: {
            if ((adx==1 && ady==2) || (adx==2 && ady==1)) return true;
            return false;
        }
        case TipoPieza::King: {
            if (adx<=1 && ady<=1) return true;
            return false;
        }
    }
    return false;
}

bool estaCasillaAtacada(const vector<Pieza>& piezas, ColorPieza colorAtacante, int f, int c){
    for (int i=0;i<(int)piezas.size();++i){
        if (!piezas[i].alive) continue;
        if (piezas[i].color != colorAtacante) continue;
        if (puedeAtacar(piezas, i, f, c)) return true;
    }
    return false;
}

// Flags de reglas especiales por jugador
struct ReglasFlags {
    bool guardiaUsado = false;           // 1 vez por juego
    int  guardiaIdx = -1;                // índice pieza protegida
    bool proteccionActiva = false;       // protección corre durante el turno completo del rival
    ColorPieza proteccionTurnoDe = ColorPieza::White; // quién está protegido este turno

    bool enroque3Usado = false;          // 1 vez por juego (enroque extendido)
};

// Core: movimiento legal (incluye enroque normal y extendido, y bloquea captura de pieza protegida)
bool movimientoLegal(const vector<Pieza>& piezas, int idx, int dstF, int dstC, const ReglasFlags& flagsBlanco, const ReglasFlags& flagsNegro){
    if (!dentroTablero(dstF,dstC)) return false;
    const Pieza &p = piezas[idx];
    if (!p.alive) return false;
    int sF = p.fila, sC = p.col;
    if (sF==dstF && sC==dstC) return false;

    // no capturar propia pieza
    if (tableroLogico[dstF][dstC] != -1){
        int occ = tableroLogico[dstF][dstC];
        if (occ >= 0 && piezas[occ].color == p.color) return false;

        // Regla 1: pieza protegida no puede ser capturada durante el turno de protección
        const ReglasFlags& flagsOponente = (p.color == ColorPieza::White) ? flagsNegro : flagsBlanco;
        if (flagsOponente.proteccionActiva && flagsOponente.guardiaIdx == occ){
            // La captura se ignora este turno: el movimiento a casilla ocupada por protegido no es legal
            return false;
        }
    }

    int dx = dstC - sC;
    int dy = dstF - sF;
    int adx = abs(dx), ady = abs(dy);

    switch(p.tipo){
        case TipoPieza::Pawn: {
            int dir = (p.color == ColorPieza::White) ? -1 : 1; // white sube (fila decrece)
            if (dx==0 && dy==dir && tableroLogico[dstF][dstC]==-1) return true;
            if (dx==0 && dy==2*dir){
                bool inicio = (p.color==ColorPieza::White)? (sF==6) : (sF==1);
                if (!inicio) return false;
                int midF = sF + dir;
                if (tableroLogico[midF][sC]==-1 && tableroLogico[dstF][dstC]==-1) return true;
                return false;
            }
            if (abs(dx)==1 && dy==dir && tableroLogico[dstF][dstC]!=-1){
                int occ = tableroLogico[dstF][dstC];
                if (occ>=0 && piezas[occ].color != p.color){
                    // también respeta protección del oponente (ya validado arriba)
                    return true;
                }
            }
            return false;
        }
        case TipoPieza::Rook: {
            if (dx!=0 && dy!=0) return false;
            if (!lineaLibre(sF,sC,dstF,dstC)) return false;
            return true;
        }
        case TipoPieza::Bishop: {
            if (adx!=ady) return false;
            if (!lineaLibre(sF,sC,dstF,dstC)) return false;
            return true;
        }
        case TipoPieza::Queen: {
            if (!((dx==0) || (dy==0) || (adx==ady))) return false;
            if (!lineaLibre(sF,sC,dstF,dstC)) return false;
            return true;
        }
        case TipoPieza::Knight: {
            if ((adx==1 && ady==2) || (adx==2 && ady==1)) return true;
            return false;
        }
        case TipoPieza::King: {
            // movimiento normal de 1 casilla
            if (adx<=1 && ady<=1) return true;

            // --- enroque normal (2 casillas) ---
            if (ady==0 && (adx==2)){
                if (p.hasMoved) return false;
                int dir = (dx>0)? 1 : -1;
                int rookCol = (dir>0) ? 7 : 0;
                if (!dentroTablero(sF, rookCol)) return false;
                int rookIdx = tableroLogico[sF][rookCol];
                if (rookIdx == -1) return false;
                const Pieza &r = piezas[rookIdx];
                if (!r.alive || r.tipo != TipoPieza::Rook || r.color != p.color || r.hasMoved) return false;

                // camino libre
                for (int cc = (dir>0? sC+1 : rookCol+1); cc < (dir>0? rookCol : sC); ++cc){
                    if (tableroLogico[sF][cc] != -1) return false;
                }

                // casillas del rey no deben estar atacadas
                ColorPieza enemigo = (p.color==ColorPieza::White)? ColorPieza::Black : ColorPieza::White;
                if (estaCasillaAtacada(piezas, enemigo, sF, sC)) return false;
                int passC = sC + dir;
                int finalC = sC + 2*dir;
                if (estaCasillaAtacada(piezas, enemigo, sF, passC)) return false;
                if (estaCasillaAtacada(piezas, enemigo, sF, finalC)) return false;
                return true;
            }

            // --- Regla 2: enroque extendido (3 casillas) una vez por jugador ---
            if (ady==0 && (adx==3)){
                // Nota: validación de "una vez por partida" se hace antes de aplicar movimiento (no aquí),
                // pero aquí comprobamos condiciones posicionales.
                if (p.hasMoved) return false;
                int dir = (dx>0)? 1 : -1;
                // Se puede enrocar con CUALQUIER torre del lado escogido siempre que no se haya movido.
                int rookCol = (dir>0) ? 7 : 0;
                if (!dentroTablero(sF, rookCol)) return false;
                int rookIdx = tableroLogico[sF][rookCol];
                if (rookIdx == -1) return false;
                const Pieza &r = piezas[rookIdx];
                if (!r.alive || r.tipo != TipoPieza::Rook || r.color != p.color || r.hasMoved) return false;

                // camino libre completo entre rey y torre
                for (int cc = (dir>0? sC+1 : rookCol+1); cc < (dir>0? rookCol : sC); ++cc){
                    if (tableroLogico[sF][cc] != -1) return false;
                }

                // casillas del rey no deben estar atacadas durante el paso
                ColorPieza enemigo = (p.color==ColorPieza::White)? ColorPieza::Black : ColorPieza::White;
                if (estaCasillaAtacada(piezas, enemigo, sF, sC)) return false;
                int passC1 = sC + dir;
                int passC2 = sC + 2*dir;
                int finalC = sC + 3*dir;
                if (estaCasillaAtacada(piezas, enemigo, sF, passC1)) return false;
                if (estaCasillaAtacada(piezas, enemigo, sF, passC2)) return false;
                if (estaCasillaAtacada(piezas, enemigo, sF, finalC)) return false;
                return true;
            }

            return false;
        }
    }
    return false;
}

int encontrarIndiceRey(const vector<Pieza>& piezas, ColorPieza color){
    for(int i=0;i<(int)piezas.size();++i){
        if (piezas[i].alive && piezas[i].tipo==TipoPieza::King && piezas[i].color==color)
            return i;
    }
    return -1;
}

bool estaEnJaque(const vector<Pieza>& piezas, ColorPieza color){
    int reyIdx = encontrarIndiceRey(piezas, color);
    if (reyIdx == -1) return false;
    int reyF = piezas[reyIdx].fila, reyC = piezas[reyIdx].col;
    if (!dentroTablero(reyF, reyC)) return false;

    for(int i=0;i<(int)piezas.size();++i){
        if (!piezas[i].alive) continue;
        if (piezas[i].color == color) continue;
        if (movimientoLegal(piezas, i, reyF, reyC, ReglasFlags{}, ReglasFlags{})) return true; // flags vacíos para ataque
    }
    return false;
}

// Simulación (respeta protección y enroque extendido)
bool dejaReyEnJaqueSimulado(vector<Pieza>& piezas, int moverIdx, int dstF, int dstC, const ReglasFlags& flagsBlanco, const ReglasFlags& flagsNegro){
    int backupTab[FILAS][COLS];
    for(int r=0;r<FILAS;r++) for(int c=0;c<COLS;c++) backupTab[r][c] = tableroLogico[r][c];
    vector<Pieza> backupPiezas = piezas;

    int srcF = piezas[moverIdx].fila;
    int srcC = piezas[moverIdx].col;

    // manejar captura si existe (bloqueada si protegido)
    int victIdx = -1;
    if (dentroTablero(dstF,dstC)) {
        victIdx = tableroLogico[dstF][dstC];
        if (victIdx != -1) {
            const ReglasFlags& flagsOponente = (piezas[moverIdx].color == ColorPieza::White) ? flagsNegro : flagsBlanco;
            if (flagsOponente.proteccionActiva && flagsOponente.guardiaIdx == victIdx) {
                // captura bloqueada -> el movimiento a esa casilla no es válido realmente
                piezas = backupPiezas;
                for(int r=0;r<FILAS;r++) for(int c=0;c<COLS;c++) tableroLogico[r][c] = backupTab[r][c];
                return true; // fuerza "en jaque" como consecuencia de movimiento inválido
            }
        }
    }

    if (dentroTablero(srcF,srcC)) tableroLogico[srcF][srcC] = -1;

    if (victIdx != -1){
        piezas[victIdx].alive = false;
        piezas[victIdx].fila = piezas[victIdx].col = -1;
    }
    piezas[moverIdx].fila = dstF;
    piezas[moverIdx].col = dstC;
    tableroLogico[dstF][dstC] = moverIdx;

    // enroque normal y extendido en simulación
    if (backupPiezas[moverIdx].tipo == TipoPieza::King && abs(dstC - srcC) >= 2){
        int dir = (dstC - srcC) > 0 ? 1 : -1;
        int rookCol = (dir>0)? 7 : 0;
        int rookIdx = backupTab[srcF][rookCol];
        if (rookIdx != -1){
            int newRookCol = (abs(dstC - srcC) == 2) ? (srcC + dir) : (srcC + 2*dir); // extendido: torre cruza 2 casillas
            if (dentroTablero(srcF, rookCol)) tableroLogico[srcF][rookCol] = -1;
            tableroLogico[srcF][newRookCol] = rookIdx;
            piezas[rookIdx].fila = srcF;
            piezas[rookIdx].col = newRookCol;
        }
    }

    ColorPieza colorMover = piezas[moverIdx].color;
    bool enJaque = estaEnJaque(piezas, colorMover);

    piezas = backupPiezas;
    for(int r=0;r<FILAS;r++) for(int c=0;c<COLS;c++) tableroLogico[r][c] = backupTab[r][c];

    return enJaque;
}

bool esJaqueMate(vector<Pieza>& piezas, ColorPieza color, const ReglasFlags& flagsBlanco, const ReglasFlags& flagsNegro){
    if (!estaEnJaque(piezas, color)) return false;

    for(int i=0;i<(int)piezas.size();++i){
        if (!piezas[i].alive) continue;
        if (piezas[i].color != color) continue;
        for(int rf=0; rf<FILAS; ++rf){
            for(int rc=0; rc<COLS; ++rc){
                if (!movimientoLegal(piezas, i, rf, rc, flagsBlanco, flagsNegro)) continue;
                if (!dejaReyEnJaqueSimulado(piezas, i, rf, rc, flagsBlanco, flagsNegro)) return false;
            }
        }
    }
    return true;
}

// ---------------------- Centrar y escalar sprite ----------------------
void centrarYescalar(sf::Sprite &s, int fila, int col){
    sf::FloatRect b = s.getLocalBounds();
    s.setOrigin(b.width/2.f, b.height/2.f);
    sf::Vector2f centro = centroCasilla(fila,col);
    s.setPosition(centro);
}

// ---------------------- MAIN ----------------------
int main(){
    sf::RenderWindow window(sf::VideoMode(1000,700), "Ajedrez SFML - Jaque & Jaque Mate (con enroque + reglas especiales)");
    window.setFramerateLimit(60);

    // Cargar texturas
    map<string,sf::Texture> tex;
    vector<pair<string,string>> lista = {
        {"PeonB","assets/images/PeonB.png"},{"PeonR","assets/images/PeonR.png"},
        {"TorreB","assets/images/TorreB.png"},{"TorreR","assets/images/TorreR.png"},
        {"CaballoB","assets/images/CaballoB.png"},{"CaballoR","assets/images/CaballoR.png"},
        {"AlfilB","assets/images/AlfilB.png"},{"AlfilR","assets/images/AlfilR.png"},
        {"DamaB","assets/images/DamaB.png"},{"DamaR","assets/images/DamaR.png"},
        {"ReyB","assets/images/ReyB.png"},{"ReyR","assets/images/ReyR.png"},
        {"Fondo","assets/images/Fondo.png"}, {"Tablero","assets/images/Tablero.png"},
        {"Escoge","assets/images/Escoge.png"} // UI de promoción
    };
    for(auto &p: lista){
        if(!cargarTxt(tex[p.first], p.second)) return -1;
    }

    sf::Sprite fondo(tex["Fondo"]);
    sf::Sprite tablero(tex["Tablero"]);
    tablero.setPosition((float)TABLERO_X, (float)TABLERO_Y);
    float escalaTab = (8.0f * TAM_CASILLA) / (float)tex["Tablero"].getSize().x;
    tablero.setScale(escalaTab, escalaTab);

    // vector de piezas
    vector<Pieza> piezas;
    piezas.reserve(32);
    for(int r=0;r<FILAS;r++) for(int c=0;c<COLS;c++) tableroLogico[r][c] = -1;

    auto idMaker = [&](const string &base, int n){ return base + "_" + to_string(n); };
    int contadorId = 0;

    auto addPieza = [&](TipoPieza tipo, ColorPieza color, int fila, int col, const string &texKey, const string &baseKey){
        Pieza p;
        p.id = idMaker(baseKey, contadorId++);
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
        piezas.push_back(move(p));
    };

    // Añadir piezas (blancas abajo)
    for(int c=0;c<8;c++) addPieza(TipoPieza::Pawn, ColorPieza::White, 6, c, "PeonB", "PeonB");
    addPieza(TipoPieza::Rook, ColorPieza::White, 7, 0, "TorreB", "TorreB");
    addPieza(TipoPieza::Knight, ColorPieza::White, 7, 1, "CaballoB", "CaballoB");
    addPieza(TipoPieza::Bishop, ColorPieza::White, 7, 2, "AlfilB", "AlfilB");
    addPieza(TipoPieza::Queen, ColorPieza::White, 7, 3, "DamaB", "DamaB");
    addPieza(TipoPieza::King, ColorPieza::White, 7, 4, "ReyB", "ReyB");
    addPieza(TipoPieza::Bishop, ColorPieza::White, 7, 5, "AlfilB", "AlfilB");
    addPieza(TipoPieza::Knight, ColorPieza::White, 7, 6, "CaballoB", "CaballoB");
    addPieza(TipoPieza::Rook, ColorPieza::White, 7, 7, "TorreB", "TorreB");

    // negras
    for(int c=0;c<8;c++) addPieza(TipoPieza::Pawn, ColorPieza::Black, 1, c, "PeonR", "PeonR");
    addPieza(TipoPieza::Rook, ColorPieza::Black, 0, 0, "TorreR", "TorreR");
    addPieza(TipoPieza::Knight, ColorPieza::Black, 0, 1, "CaballoR", "CaballoR");
    addPieza(TipoPieza::Bishop, ColorPieza::Black, 0, 2, "AlfilR", "AlfilR");
    addPieza(TipoPieza::Queen, ColorPieza::Black, 0, 3, "DamaR", "DamaR");
    addPieza(TipoPieza::King, ColorPieza::Black, 0, 4, "ReyR", "ReyR");
    addPieza(TipoPieza::Bishop, ColorPieza::Black, 0, 5, "AlfilR", "AlfilR");
    addPieza(TipoPieza::Knight, ColorPieza::Black, 0, 6, "CaballoR", "CaballoR");
    addPieza(TipoPieza::Rook, ColorPieza::Black, 0, 7, "TorreR", "TorreR");

    // Flags por jugador
    ReglasFlags flagsBlanco, flagsNegro;

    // interacción
    bool arrastrando = false;
    int idxSeleccionado = -1;
    sf::Vector2f difMouse;
    int origenF=-1, origenC=-1;
    ColorPieza turno = ColorPieza::White;
    vector<pair<int,int>> movimientosValidos;

    // Estado de promoción (Regla 3)
    bool mostrandoPromocion = false;
    int idxPeonPromocion = -1;
    sf::Sprite recuadroPromocion(tex["Escoge"]);
    // Centrar recuadro en la ventana
    {
        sf::Vector2u sizeTabla = tex["Escoge"].getSize();
        float escalaX = 1.0f, escalaY = 1.0f;
        recuadroPromocion.setOrigin(sizeTabla.x/2.0f, sizeTabla.y/2.0f);
        recuadroPromocion.setPosition(1000/2.0f, 700/2.0f);
        recuadroPromocion.setScale(escalaX, escalaY);
    }
    // Botones de piezas dentro del recuadro (posiciones relativas simples)
    sf::Sprite btnRook, btnKnight, btnBishop, btnQueen;
    auto configurarBotonesPromocion = [&](ColorPieza color){
        btnRook.setTexture(tex[(color==ColorPieza::White)?"TorreB":"TorreR"]);
        btnKnight.setTexture(tex[(color==ColorPieza::White)?"CaballoB":"CaballoR"]);
        btnBishop.setTexture(tex[(color==ColorPieza::White)?"AlfilB":"AlfilR"]);
        btnQueen.setTexture(tex[(color==ColorPieza::White)?"DamaB":"DamaR"]);
        // Escala a tamaño casilla
        auto setScaleFor = [&](sf::Sprite& s){
            const sf::Texture* t = s.getTexture();
            if (t){
                float sx = (float)TAM_CASILLA / (float)t->getSize().x;
                float sy = (float)TAM_CASILLA / (float)t->getSize().y;
                s.setScale(sx, sy);
            }
        };
        setScaleFor(btnRook); setScaleFor(btnKnight); setScaleFor(btnBishop); setScaleFor(btnQueen);

        sf::Vector2f center = recuadroPromocion.getPosition();
        float spacing = 90.0f;
        btnRook.setPosition(center.x - 1.5f*spacing, center.y);
        btnKnight.setPosition(center.x - 0.5f*spacing, center.y);
        btnBishop.setPosition(center.x + 0.5f*spacing, center.y);
        btnQueen.setPosition(center.x + 1.5f*spacing, center.y);
    };

    // sombra para arrastre
    sf::CircleShape sombra((float)TAM_CASILLA * 0.45f);
    sombra.setFillColor(sf::Color(0,0,0,120));
    sombra.setOrigin(sombra.getRadius(), sombra.getRadius());

    auto aplicarPromocion = [&](TipoPieza nuevoTipo){
        if (idxPeonPromocion < 0 || idxPeonPromocion >= (int)piezas.size()) return;
        Pieza &peon = piezas[idxPeonPromocion];
        if (!peon.alive || peon.tipo != TipoPieza::Pawn) return;

        // Cambiar tipo y textura
        peon.tipo = nuevoTipo;
        string texKey;
        if (nuevoTipo == TipoPieza::Rook)   texKey = (peon.color==ColorPieza::White)?"TorreB":"TorreR";
        if (nuevoTipo == TipoPieza::Knight) texKey = (peon.color==ColorPieza::White)?"CaballoB":"CaballoR";
        if (nuevoTipo == TipoPieza::Bishop) texKey = (peon.color==ColorPieza::White)?"AlfilB":"AlfilR";
        if (nuevoTipo == TipoPieza::Queen)  texKey = (peon.color==ColorPieza::White)?"DamaB":"DamaR";
        peon.sprite.setTexture(tex[texKey]);

        const sf::Texture* t = peon.sprite.getTexture();
        if (t){
            peon.baseSx = (float)TAM_CASILLA / (float)t->getSize().x;
            peon.baseSy = (float)TAM_CASILLA / (float)t->getSize().y;
            peon.sprite.setScale(peon.baseSx, peon.baseSy);
        }
        centrarYescalar(peon.sprite, peon.fila, peon.col);

        mostrandoPromocion = false;
        idxPeonPromocion = -1;
    };

    // bucle principal
    while(window.isOpen()){
        sf::Event ev;
        while(window.pollEvent(ev)){
            if(ev.type==sf::Event::Closed) window.close();

            // Si se está mostrando la promoción, solo manejar clicks sobre los botones
            if (mostrandoPromocion){
                if (ev.type == sf::Event::MouseButtonPressed && ev.mouseButton.button == sf::Mouse::Left){
                    sf::Vector2f mpos = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                    if (btnRook.getGlobalBounds().contains(mpos))   aplicarPromocion(TipoPieza::Rook);
                    else if (btnKnight.getGlobalBounds().contains(mpos)) aplicarPromocion(TipoPieza::Knight);
                    else if (btnBishop.getGlobalBounds().contains(mpos)) aplicarPromocion(TipoPieza::Bishop);
                    else if (btnQueen.getGlobalBounds().contains(mpos))  aplicarPromocion(TipoPieza::Queen);
                }
                // Bloquear otros eventos mientras la UI está activa
                continue;
            }

            // PRESionar
            if(ev.type==sf::Event::MouseButtonPressed && ev.mouseButton.button==sf::Mouse::Left){
                sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                idxSeleccionado = -1;
                for(int i=(int)piezas.size()-1;i>=0;--i){
                    if (!piezas[i].alive) continue;
                    if (piezas[i].sprite.getGlobalBounds().contains(mouse)){
                        // validar turno
                        if ((turno == ColorPieza::White && piezas[i].color != ColorPieza::White) ||
                            (turno == ColorPieza::Black && piezas[i].color != ColorPieza::Black)){
                            idxSeleccionado = -1;
                        } else {
                            idxSeleccionado = i;
                        }
                        break;
                    }
                }
                if (idxSeleccionado != -1){
                    arrastrando = true;
                    difMouse = mouse - piezas[idxSeleccionado].sprite.getPosition();
                    origenF = piezas[idxSeleccionado].fila;
                    origenC = piezas[idxSeleccionado].col;

                    // movimientos válidos (con reglas especiales)
                    movimientosValidos.clear();
                    for(int rf=0; rf<FILAS; ++rf){
                        for(int rc=0; rc<COLS; ++rc){
                            if (movimientoLegal(piezas, idxSeleccionado, rf, rc, flagsBlanco, flagsNegro))
                                movimientosValidos.emplace_back(rf, rc);
                        }
                    }

                    // animación "levantar"
                    piezas[idxSeleccionado].sprite.setScale(piezas[idxSeleccionado].baseSx * 1.15f,
                                                           piezas[idxSeleccionado].baseSy * 1.15f);
                }
            }

            // Regla 1: activar "guardia" con tecla G sobre la pieza seleccionada (una vez por jugador)
            if (ev.type == sf::Event::KeyPressed && ev.key.code == sf::Keyboard::G && idxSeleccionado != -1){
                ReglasFlags &flags = (turno==ColorPieza::White)? flagsBlanco : flagsNegro;
                if (!flags.guardiaUsado){
                    flags.guardiaUsado = true;
                    flags.guardiaIdx = idxSeleccionado;
                    piezas[idxSeleccionado].protegido = true;
                    flags.proteccionActiva = true;
                    flags.proteccionTurnoDe = turno;
                }
            }

            // SOLTAR
            if(ev.type==sf::Event::MouseButtonReleased && ev.mouseButton.button==sf::Mouse::Left && arrastrando && idxSeleccionado != -1){
                arrastrando = false;
                sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
                auto [dstF, dstC] = casillaMasCercana(mouse.x, mouse.y);

                // movimiento permitido por radio (tolerancia)
                sf::Vector2f centro = centroCasilla(dstF, dstC);
                float dist = hypotf(mouse.x - centro.x, mouse.y - centro.y);

                bool dentroRadio = (dist <= RADIO_ACEPTACION);
                bool legal = movimientoLegal(piezas, idxSeleccionado, dstF, dstC, flagsBlanco, flagsNegro);

                // restaurar escala
                piezas[idxSeleccionado].sprite.setScale(piezas[idxSeleccionado].baseSx, piezas[idxSeleccionado].baseSy);

                if (dentroRadio && legal){
                    if (!dejaReyEnJaqueSimulado(piezas, idxSeleccionado, dstF, dstC, flagsBlanco, flagsNegro)){
                        // captura (si no protegido)
                        if (tableroLogico[dstF][dstC] != -1){
                            int victim = tableroLogico[dstF][dstC];
                            const ReglasFlags& flagsOponente = (piezas[idxSeleccionado].color == ColorPieza::White)? flagsNegro : flagsBlanco;
                            if (!(flagsOponente.proteccionActiva && flagsOponente.guardiaIdx == victim)){
                                if (victim >= 0 && piezas[victim].alive && piezas[victim].color != piezas[idxSeleccionado].color){
                                    piezas[victim].animandoCaptura = true;
                                    piezas[victim].animClock.restart();
                                }
                            } else {
                                // protegido: no capturamos (ya lo bloqueó movimientoLegal, seguridad adicional)
                            }
                        }

                        bool fueEnroque = false;
                        int moveCols = abs(dstC - origenC);

                        if (piezas[idxSeleccionado].tipo == TipoPieza::King && (moveCols == 2 || moveCols == 3)){
                            int dir = (dstC - origenC) > 0 ? 1 : -1;
                            int rookCol = (dir>0)? 7 : 0;
                            int rookIdx = tableroLogico[origenF][rookCol];
                            if (rookIdx != -1){
                                // borrar origen del rey
                                if (dentroTablero(origenF,origenC)) tableroLogico[origenF][origenC] = -1;
                                piezas[idxSeleccionado].fila = dstF; piezas[idxSeleccionado].col = dstC;
                                centrarYescalar(piezas[idxSeleccionado].sprite, dstF, dstC);
                                tableroLogico[dstF][dstC] = idxSeleccionado;
                                piezas[idxSeleccionado].hasMoved = true;

                                // mover torre
                                int newRookCol = (moveCols==2) ? (origenC + dir) : (origenC + 2*dir);
                                if (dentroTablero(origenF, rookCol)) tableroLogico[origenF][rookCol] = -1;
                                piezas[rookIdx].fila = origenF;
                                piezas[rookIdx].col = newRookCol;
                                centrarYescalar(piezas[rookIdx].sprite, piezas[rookIdx].fila, piezas[rookIdx].col);
                                tableroLogico[origenF][newRookCol] = rookIdx;
                                piezas[rookIdx].hasMoved = true;

                                fueEnroque = true;

                                // Regla 2: registrar uso de enroque extendido
                                if (moveCols == 3){
                                    ReglasFlags &flags = (turno==ColorPieza::White)? flagsBlanco : flagsNegro;
                                    flags.enroque3Usado = true;
                                }
                            }
                        }

                        if (!fueEnroque){
                            // movimiento normal
                            if (dentroTablero(origenF,origenC)) tableroLogico[origenF][origenC] = -1;
                            piezas[idxSeleccionado].fila = dstF; piezas[idxSeleccionado].col = dstC;
                            centrarYescalar(piezas[idxSeleccionado].sprite, dstF, dstC);
                            tableroLogico[dstF][dstC] = idxSeleccionado;
                            piezas[idxSeleccionado].hasMoved = true;
                        }

                        // Regla 3: promoción automática al llegar a última fila
                        if (piezas[idxSeleccionado].tipo == TipoPieza::Pawn){
                            bool llegoUltima = (piezas[idxSeleccionado].color==ColorPieza::White)? (dstF==0) : (dstF==7);
                            if (llegoUltima){
                                mostrandoPromocion = true;
                                idxPeonPromocion = idxSeleccionado;
                                configurarBotonesPromocion(piezas[idxSeleccionado].color);
                            }
                        }

                        // Cambiar turno
                        turno = (turno == ColorPieza::White) ? ColorPieza::Black : ColorPieza::White;

                        // Regla 1: finalizar protección al cerrar el turno del protegido
                        // La protección dura "un turno completo" del rival. Al cambiar el turno, si la protección
                        // pertenece al jugador que ahora empieza, se desactiva (ha pasado el turno del rival).
                        {
                            ReglasFlags &fb = flagsBlanco;
                            ReglasFlags &fn = flagsNegro;
                            if (fb.proteccionActiva && fb.proteccionTurnoDe != turno){
                                // La protección era de blancas y acaba de terminar el turno de negras -> desactivar
                                if (fb.guardiaIdx >=0 && fb.guardiaIdx < (int)piezas.size()) piezas[fb.guardiaIdx].protegido = false;
                                fb.proteccionActiva = false;
                            }
                            if (fn.proteccionActiva && fn.proteccionTurnoDe != turno){
                                if (fn.guardiaIdx >=0 && fn.guardiaIdx < (int)piezas.size()) piezas[fn.guardiaIdx].protegido = false;
                                fn.proteccionActiva = false;
                            }
                        }

                    } else {
                        // movimiento deja rey en jaque -> revertir
                        piezas[idxSeleccionado].fila = origenF; piezas[idxSeleccionado].col = origenC;
                        centrarYescalar(piezas[idxSeleccionado].sprite, origenF, origenC);
                        if (dentroTablero(origenF,origenC)) tableroLogico[origenF][origenC] = idxSeleccionado;
                    }
                } else {
                    // fuera radio o ilegal -> revertir
                    piezas[idxSeleccionado].fila = origenF; piezas[idxSeleccionado].col = origenC;
                    centrarYescalar(piezas[idxSeleccionado].sprite, origenF, origenC);
                    if (dentroTablero(origenF,origenC)) tableroLogico[origenF][origenC] = idxSeleccionado;
                }

                movimientosValidos.clear();
                idxSeleccionado = -1;
            }
        } // events

        // arrastre visual
        if (arrastrando && idxSeleccionado != -1){
            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            piezas[idxSeleccionado].sprite.setPosition(mouse - difMouse);
        }

        // animaciones de captura
        for (int i=0;i<(int)piezas.size();++i){
            if (piezas[i].animandoCaptura){
                float t = piezas[i].animClock.getElapsedTime().asSeconds() / DURACION_ANIMACION_CAPTURA;
                if (t >= 1.0f){
                    piezas[i].animandoCaptura = false;
                    piezas[i].alive = false;
                    if (piezas[i].fila >=0 && piezas[i].col >=0 && tableroLogico[piezas[i].fila][piezas[i].col] == i)
                        tableroLogico[piezas[i].fila][piezas[i].col] = -1;
                    piezas[i].fila = piezas[i].col = -1;
                    piezas[i].sprite.setPosition(-2000.f, -2000.f);
                } else {
                    float s = 1.0f - t;
                    const sf::Texture* tx = piezas[i].sprite.getTexture();
                    if (tx){
                        piezas[i].sprite.setScale(piezas[i].baseSx * s, piezas[i].baseSy * s);
                    }
                    sf::Color col = piezas[i].sprite.getColor();
                    col.a = (sf::Uint8)(255 * (1.0f - t));
                    piezas[i].sprite.setColor(col);
                }
            }
        }

        // Jaque / mate
        bool blancoEnJaque = estaEnJaque(piezas, ColorPieza::White);
        bool negroEnJaque  = estaEnJaque(piezas, ColorPieza::Black);
        bool blancoJaqueMate = esJaqueMate(piezas, ColorPieza::White, flagsBlanco, flagsNegro);
        bool negroJaqueMate  = esJaqueMate(piezas, ColorPieza::Black, flagsBlanco, flagsNegro);

        // dibujado
        window.clear();
        window.draw(fondo);
        window.draw(tablero);

        // dots
        sf::CircleShape dot((float)TAM_CASILLA * 0.12f);
        dot.setOrigin(dot.getRadius(), dot.getRadius());
        dot.setFillColor(DOT_COLOR);
        for (auto &m : movimientosValidos){
            sf::Vector2f c = centroCasilla(m.first, m.second);
            dot.setPosition(c);
            window.draw(dot);
        }

        // resaltar rey en jaque
        if (blancoEnJaque || negroEnJaque){
            int idxRey = -1;
            ColorPieza c = blancoEnJaque ? ColorPieza::White : ColorPieza::Black;
            idxRey = encontrarIndiceRey(piezas, c);
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

        // dibujar piezas
        for (int i=0;i<(int)piezas.size();++i){
            if (i == idxSeleccionado) continue;
            if (piezas[i].alive || piezas[i].animandoCaptura) window.draw(piezas[i].sprite);
        }

        // pieza arrastrada + sombra
        if (idxSeleccionado != -1 && piezas[idxSeleccionado].alive){
            sf::Vector2f pos = piezas[idxSeleccionado].sprite.getPosition();
            sombra.setPosition(pos.x + 6.f, pos.y + 10.f);
            window.draw(sombra);
            window.draw(piezas[idxSeleccionado].sprite);
        }

        // UI de promoción (Regla 3): oscurecer fondo y dibujar recuadro + botones
        if (mostrandoPromocion){
            sf::RectangleShape overlay(sf::Vector2f(1000.f, 700.f));
            overlay.setFillColor(sf::Color(0,0,0,150));
            overlay.setPosition(0.f,0.f);
            window.draw(overlay);

            window.draw(recuadroPromocion);
            window.draw(btnRook);
            window.draw(btnKnight);
            window.draw(btnBishop);
            window.draw(btnQueen);
        }

        window.display();
    } // loop

    return 0;
}
