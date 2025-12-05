// Juego.cpp
// Ajedrez SFML - Con jaque y detección de jaque mate
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
// Comprueba si la línea recta entre (f1,c1) y (f2,c2) está libre (excluye origen y destino)
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

// Core: determina si un movimiento (idx -> dstF,dstC) es legal según reglas (bloqueos y captura).
// No considera dejar rey en jaque; eso se revisa por separado.
bool movimientoLegal(const vector<Pieza>& piezas, int idx, int dstF, int dstC){
    if (!dentroTablero(dstF,dstC)) return false;
    const Pieza &p = piezas[idx];
    if (!p.alive) return false;
    int sF = p.fila, sC = p.col;
    if (sF==dstF && sC==dstC) return false;

    // no capturar propia pieza
    if (tableroLogico[dstF][dstC] != -1){
        int occ = tableroLogico[dstF][dstC];
        if (occ >= 0 && piezas[occ].color == p.color) return false;
    }

    int dx = dstC - sC;
    int dy = dstF - sF;
    int adx = abs(dx), ady = abs(dy);

    switch(p.tipo){
        case TipoPieza::Pawn: {
            int dir = (p.color == ColorPieza::White) ? -1 : 1; // white sube (fila decrece)
            // avance uno
            if (dx==0 && dy==dir && tableroLogico[dstF][dstC]==-1) return true;
            // doble avance desde inicio
            if (dx==0 && dy==2*dir){
                bool inicio = (p.color==ColorPieza::White)? (sF==6) : (sF==1);
                if (!inicio) return false;
                int midF = sF + dir;
                if (tableroLogico[midF][sC]==-1 && tableroLogico[dstF][dstC]==-1) return true;
                return false;
            }
            // captura diagonal
            if (abs(dx)==1 && dy==dir && tableroLogico[dstF][dstC]!=-1){
                int occ = tableroLogico[dstF][dstC];
                if (occ>=0 && piezas[occ].color != p.color) return true;
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
            if (adx<=1 && ady<=1) return true;
            return false;
        }
    }
    return false;
}

// Encuentra índice del rey de color 'color' en piezas, -1 si no existe
int encontrarIndiceRey(const vector<Pieza>& piezas, ColorPieza color){
    for(int i=0;i<(int)piezas.size();++i){
        if (piezas[i].alive && piezas[i].tipo==TipoPieza::King && piezas[i].color==color)
            return i;
    }
    return -1;
}

// Determina si 'color' está en jaque (false si no hay rey)
bool estaEnJaque(const vector<Pieza>& piezas, ColorPieza color){
    int reyIdx = encontrarIndiceRey(piezas, color);
    if (reyIdx == -1) return false; // sin rey, no consideramos jaque
    int reyF = piezas[reyIdx].fila, reyC = piezas[reyIdx].col;
    if (!dentroTablero(reyF, reyC)) return false;

    // cualquier pieza del bando contrario que pueda mover a la casilla del rey produce jaque
    for(int i=0;i<(int)piezas.size();++i){
        if (!piezas[i].alive) continue;
        if (piezas[i].color == color) continue;
        if (movimientoLegal(piezas, i, reyF, reyC)) return true;
    }
    return false;
}

// Simula el movimiento idx -> (dstF,dstC) y devuelve true si tras el movimiento el propio rey queda en jaque.
// IMPORTANTE: modifica temporalmente tableroLogico y piezas, y restaura antes de retornar.
bool dejaReyEnJaqueSimulado(vector<Pieza>& piezas, int moverIdx, int dstF, int dstC){
    // respaldos
    int backupTab[FILAS][COLS];
    for(int r=0;r<FILAS;r++) for(int c=0;c<COLS;c++) backupTab[r][c] = tableroLogico[r][c];
    vector<Pieza> backupPiezas = piezas;

    // datos origen
    int srcF = piezas[moverIdx].fila;
    int srcC = piezas[moverIdx].col;

    // manejar captura si existe
    int victIdx = -1;
    if (dentroTablero(dstF,dstC)) victIdx = tableroLogico[dstF][dstC];

    // aplicar move en "piezas" y tableroLogico
    // quitar origen
    if (dentroTablero(srcF,srcC)) tableroLogico[srcF][srcC] = -1;
    // si victima existe, marcarla como muerta y quitar de tablero
    if (victIdx != -1){
        piezas[victIdx].alive = false;
        piezas[victIdx].fila = piezas[victIdx].col = -1;
        // tableroLogico[dstF][dstC] sobreescribiremos con moverIdx
    }
    // mover pieza
    piezas[moverIdx].fila = dstF;
    piezas[moverIdx].col = dstC;
    tableroLogico[dstF][dstC] = moverIdx;

    // encontrar rey del color del que movimos
    ColorPieza colorMover = piezas[moverIdx].color;
    bool enJaque = estaEnJaque(piezas, colorMover);

    // restaurar
    piezas = backupPiezas;
    for(int r=0;r<FILAS;r++) for(int c=0;c<COLS;c++) tableroLogico[r][c] = backupTab[r][c];

    return enJaque;
}

// Comprueba si el jugador 'color' está en jaque mate
bool esJaqueMate(vector<Pieza>& piezas, ColorPieza color){
    if (!estaEnJaque(piezas, color)) return false; // no hay jaque -> no mate

    // para cada pieza del color, probar todos sus movimientos legales; si alguno evita el jaque => no mate
    for(int i=0;i<(int)piezas.size();++i){
        if (!piezas[i].alive) continue;
        if (piezas[i].color != color) continue;
        // generar destinos posibles (8x8)
        for(int rf=0; rf<FILAS; ++rf){
            for(int rc=0; rc<COLS; ++rc){
                if (!movimientoLegal(piezas, i, rf, rc)) continue;
                // simular: si tras mover, el rey no está en jaque -> no mate
                if (!dejaReyEnJaqueSimulado(piezas, i, rf, rc)) return false;
            }
        }
    }
    return true; // ningún movimiento válido salva al rey => jaque mate
}

// ---------------------- Centrar y escalar sprite ----------------------
// Ahora centrarYescalar solo coloca el origen y la posición; la escala base se asigna desde addPieza
void centrarYescalar(sf::Sprite &s, int fila, int col){
    sf::FloatRect b = s.getLocalBounds();
    s.setOrigin(b.width/2.f, b.height/2.f);
    sf::Vector2f centro = centroCasilla(fila,col);
    s.setPosition(centro);
}

// ---------------------- MAIN ----------------------
int main(){
    sf::RenderWindow window(sf::VideoMode(1000,700), "Ajedrez SFML - Jaque & Jaque Mate");
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
        {"Fondo","assets/images/Fondo.png"}, {"Tablero","assets/images/Tablero.png"}
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

    // addPieza ahora calcula y guarda baseSx/baseSy
    auto addPieza = [&](TipoPieza tipo, ColorPieza color, int fila, int col, const string &texKey, const string &baseKey){
        Pieza p;
        p.id = idMaker(baseKey, contadorId++);
        p.tipo = tipo;
        p.color = color;
        p.fila = fila;
        p.col = col;
        p.sprite.setTexture(tex[texKey]);

        // escala base (guardarla para futuras animaciones)
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

    // interacción
    bool arrastrando = false;
    int idxSeleccionado = -1;
    sf::Vector2f difMouse;
    int origenF=-1, origenC=-1;
    ColorPieza turno = ColorPieza::White;
    vector<pair<int,int>> movimientosValidos;

    // texto (fuente opcional)
    sf::Font font;
    bool fontOk = font.loadFromFile("assets/fonts/arial.ttf");
    sf::Text textoEstado;
    if (fontOk){
        textoEstado.setFont(font);
        textoEstado.setCharacterSize(24);
        textoEstado.setFillColor(sf::Color::Yellow);
        textoEstado.setPosition(10.f, 10.f);
    }

    // sombra para arrastre
    sf::CircleShape sombra((float)TAM_CASILLA * 0.45f);
    sombra.setFillColor(sf::Color(0,0,0,120));
    sombra.setOrigin(sombra.getRadius(), sombra.getRadius());

    // bucle principal
    while(window.isOpen()){
        sf::Event ev;
        while(window.pollEvent(ev)){
            if(ev.type==sf::Event::Closed) window.close();

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

                    // **NO BORRAR** tableroLogico[origenF][origenC] aquí — lo haremos solo si se confirma el movimiento

                    // calcular movimientos válidos (según reglas básicas, sin considerar dejar rey en jaque)
                    movimientosValidos.clear();
                    for(int rf=0; rf<FILAS; ++rf){
                        for(int rc=0; rc<COLS; ++rc){
                            if (movimientoLegal(piezas, idxSeleccionado, rf, rc))
                                movimientosValidos.emplace_back(rf, rc);
                        }
                    }

                    // animación de "levantar": agrandar ligeramente la pieza
                    piezas[idxSeleccionado].sprite.setScale(piezas[idxSeleccionado].baseSx * 1.15f,
                                                           piezas[idxSeleccionado].baseSy * 1.15f);
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
                bool legal = movimientoLegal(piezas, idxSeleccionado, dstF, dstC);

                // restaurar escala base por defecto; si se hizo captura, animación posterior la modificará
                piezas[idxSeleccionado].sprite.setScale(piezas[idxSeleccionado].baseSx, piezas[idxSeleccionado].baseSy);

                // no permitir mover si no legal o fuera de radio
                if (dentroRadio && legal){
                    // antes de aplicar, comprobar si dejaría al rey en jaque
                    if (!dejaReyEnJaqueSimulado(piezas, idxSeleccionado, dstF, dstC)){
                        // captura: iniciar animación en víctima si existe
                        if (tableroLogico[dstF][dstC] != -1){
                            int victim = tableroLogico[dstF][dstC];
                            if (victim >= 0 && piezas[victim].alive && piezas[victim].color != piezas[idxSeleccionado].color){
                                piezas[victim].animandoCaptura = true;
                                piezas[victim].animClock.restart();
                            }
                        }
                        // aplicar movimiento lógico (AHORA sí borramos el origen)
                        if (dentroTablero(origenF,origenC)) tableroLogico[origenF][origenC] = -1;
                        piezas[idxSeleccionado].fila = dstF; piezas[idxSeleccionado].col = dstC;
                        centrarYescalar(piezas[idxSeleccionado].sprite, dstF, dstC);
                        tableroLogico[dstF][dstC] = idxSeleccionado;

                        // cambiar turno
                        turno = (turno == ColorPieza::White) ? ColorPieza::Black : ColorPieza::White;
                    } else {
                        // movimiento dejaría rey en jaque -> revertir
                        piezas[idxSeleccionado].fila = origenF; piezas[idxSeleccionado].col = origenC;
                        centrarYescalar(piezas[idxSeleccionado].sprite, origenF, origenC);
                        if (dentroTablero(origenF,origenC)) tableroLogico[origenF][origenC] = idxSeleccionado;
                    }
                } else {
                    // fuera radio o movimiento ilegal -> revertir
                    piezas[idxSeleccionado].fila = origenF; piezas[idxSeleccionado].col = origenC;
                    centrarYescalar(piezas[idxSeleccionado].sprite, origenF, origenC);
                    if (dentroTablero(origenF,origenC)) tableroLogico[origenF][origenC] = idxSeleccionado;
                }

                // limpiar
                movimientosValidos.clear();
                idxSeleccionado = -1;
            }
        } // events

        // arrastre visual
        if (arrastrando && idxSeleccionado != -1){
            sf::Vector2f mouse = window.mapPixelToCoords(sf::Mouse::getPosition(window));
            piezas[idxSeleccionado].sprite.setPosition(mouse - difMouse);
        }

        // actualizar animaciones de captura
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

        // Determinar jaque / jaque mate para ambos bandos
        bool blancoEnJaque = estaEnJaque(piezas, ColorPieza::White);
        bool negroEnJaque  = estaEnJaque(piezas, ColorPieza::Black);
        bool blancoJaqueMate = esJaqueMate(piezas, ColorPieza::White);
        bool negroJaqueMate  = esJaqueMate(piezas, ColorPieza::Black);

        if (blancoJaqueMate) { cout << "JAQUE MATE: Negras ganan\n"; }
        if (negroJaqueMate)  { cout << "JAQUE MATE: Blancas ganan\n"; }

        // dibujado
        window.clear();
        window.draw(fondo);
        window.draw(tablero);

        // dibujar dots de movimientos válidos (para la pieza seleccionada)
        sf::CircleShape dot((float)TAM_CASILLA * 0.12f);
        dot.setOrigin(dot.getRadius(), dot.getRadius());
        dot.setFillColor(DOT_COLOR);
        for (auto &m : movimientosValidos){
            sf::Vector2f c = centroCasilla(m.first, m.second);
            dot.setPosition(c);
            window.draw(dot);
        }

        // si rey en jaque, dibujar rectángulo rojo en su casilla
        if (blancoEnJaque || negroEnJaque){
            int idxRey = -1;
            ColorPieza c = blancoEnJaque ? ColorPieza::White : ColorPieza::Black;
            idxRey = encontrarIndiceRey(piezas, c);
            if (idxRey != -1 && piezas[idxRey].alive){
                sf::RectangleShape r(sf::Vector2f((float)TAM_CASILLA, (float)TAM_CASILLA));
                r.setFillColor(sf::Color::Transparent);
                r.setOutlineColor(sf::Color::Red);
                r.setOutlineThickness(3.0f);
                // calcular esquina superior izquierda de la casilla
                float left = TABLERO_X + piezas[idxRey].col * TAM_CASILLA;
                float top  = TABLERO_Y + piezas[idxRey].fila * TAM_CASILLA;
                r.setPosition(left, top);
                window.draw(r);
            }
        }

        // dibujar piezas (excepto la arrastrada), respetando orden: primero todas menos arrastrada
        for (int i=0;i<(int)piezas.size();++i){
            if (i == idxSeleccionado) continue;
            if (piezas[i].alive || piezas[i].animandoCaptura) window.draw(piezas[i].sprite);
        }

        // Si hay una pieza arrastrada, dibujar sombra y luego la pieza para que quede encima
        if (idxSeleccionado != -1 && piezas[idxSeleccionado].alive){
            sf::Vector2f pos = piezas[idxSeleccionado].sprite.getPosition();
            sombra.setPosition(pos.x + 6.f, pos.y + 10.f); // ligero offset de sombra
            window.draw(sombra);
            window.draw(piezas[idxSeleccionado].sprite);
        }

        // dibujar texto de estado si fuente disponible
        if (fontOk){
            string s="";
            if (blancoJaqueMate) s = "JAQUE MATE - Negras ganan";
            else if (negroJaqueMate) s = "JAQUE MATE - Blancas ganan";
            else if (blancoEnJaque) s = "JAQUE - Blancas";
            else if (negroEnJaque) s = "JAQUE - Negras";
            else s = (turno == ColorPieza::White) ? "Turno: Blancas" : "Turno: Negras";
            textoEstado.setString(s);
            window.draw(textoEstado);
        } else {
            // si no hay fuente, imprimimos por consola (no frecuente)
            static int frameCnt = 0;
            if (++frameCnt % 60 == 0) {
                if (blancoJaqueMate) cout << "JAQUE MATE - Negras ganan\n";
                else if (negroJaqueMate) cout << "JAQUE MATE - Blancas ganan\n";
                else if (blancoEnJaque) cout << "JAQUE - Blancas\n";
                else if (negroEnJaque) cout << "JAQUE - Negras\n";
            }
        }

        window.display();
    } // loop

    return 0;
}

