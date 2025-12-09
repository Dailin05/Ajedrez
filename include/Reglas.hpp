
#pragma once
#include "Tipos.hpp"
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <iostream>

// ---------------------- Helpers ----------------------
inline bool dentroTablero(int f,int c){ return f>=0 && f<FILAS && c>=0 && c<COLS; }

inline sf::Vector2f centroCasilla(int fila,int col){
    float x = TABLERO_X + col * TAM_CASILLA + TAM_CASILLA / 2.0f;
    float y = TABLERO_Y + fila * TAM_CASILLA + TAM_CASILLA / 2.0f;
    return {x,y};
}

inline std::pair<int,int> casillaMasCercana(float px, float py){
    float fx = (px - TABLERO_X) / (float)TAM_CASILLA;
    float fy = (py - TABLERO_Y) / (float)TAM_CASILLA;
    int c = (int)floor(fx + 0.5f);
    int r = (int)floor(fy + 0.5f);
    if (r<0) r=0; if (r>FILAS-1) r=FILAS-1;
    if (c<0) c=0; if (c>COLS-1) c=COLS-1;
    return {r,c};
}

inline bool cargarTxt(sf::Texture &t, const std::string &ruta){
    if(!t.loadFromFile(ruta)){
        std::cerr << "No se pudo cargar: " << ruta << "\n";
        return false;
    }
    return true;
}

// ---------------------- Movimiento / reglas ----------------------
// Comprueba si la línea recta entre (f1,c1) y (f2,c2) está libre (excluye origen y destino)
inline bool lineaLibre(int f1,int c1,int f2,int c2){
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

// Versión ligera para comprobar si la pieza de índice 'attIdx' podría atacar la casilla (f,c).
// Esta función evita considerar el enroque y usa reglas de ataque (peón ataca diagonales).
inline bool puedeAtacar(const std::vector<Pieza>& piezas, int attIdx, int f, int c){
    if (attIdx < 0 || attIdx >= (int)piezas.size()) return false;
    const Pieza &p = piezas[attIdx];
    if (!p.alive) return false;
    int sF = p.fila, sC = p.col;
    if (!dentroTablero(sF,sC)) return false;
    int dx = c - sC;
    int dy = f - sF;
    int adx = std::abs(dx), ady = std::abs(dy);

    switch(p.tipo){
        case TipoPieza::Pawn: {
            int dir = (p.color == ColorPieza::White) ? -1 : 1;
            // atacan solo diagonales una casilla
            if (ady==1 && adx==1 && dy==dir) return true;
            return false;
        }
        case TipoPieza::Rook: {
            if (dx!=0 && dy!=0) return false;
            // comprobar camino libre (excluye origen/destino)
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

// Devuelve true si la casilla (f,c) está siendo atacada por alguna pieza del color 'colorAtacante'
inline bool estaCasillaAtacada(const std::vector<Pieza>& piezas, ColorPieza colorAtacante, int f, int c){
    for (int i=0;i<(int)piezas.size();++i){
        if (!piezas[i].alive) continue;
        if (piezas[i].color != colorAtacante) continue;
        if (puedeAtacar(piezas, i, f, c)) return true;
    }
    return false;
}

// Core: determina si un movimiento (idx -> dstF,dstC) es legal según reglas (bloqueos y captura).
// No considera dejar rey en jaque; eso se revisa por separado.
inline bool movimientoLegal(const std::vector<Pieza>& piezas, int idx, int dstF, int dstC){
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
    int adx = std::abs(dx), ady = std::abs(dy);

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
            if (std::abs(dx)==1 && dy==dir && tableroLogico[dstF][dstC]!=-1){
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
            // movimiento normal de 1 casilla
            if (adx<=1 && ady<=1) return true;

            // --- enroque ---
            if (ady==0 && (adx==2)){
                // condiciones básicas: rey y torre no se han movido, camino libre, y casillas no atacadas
                if (p.hasMoved) return false;
                int dir = (dx>0)? 1 : -1; // +1 enroque corto, -1 enroque largo
                int rookCol = (dir>0) ? 7 : 0;
                int rookF = sF;
                // verificar torre en su sitio
                if (!dentroTablero(sF, rookCol)) return false;
                int rookIdx = tableroLogico[sF][rookCol];
                if (rookIdx == -1) return false;
                const Pieza &r = piezas[rookIdx];
                if (!r.alive) return false;
                if (r.tipo != TipoPieza::Rook) return false;
                if (r.color != p.color) return false;
                if (r.hasMoved) return false;
                // verificar que las casillas entre rey y torre estén libres
                int betweenC1 = sC + dir;
                int betweenC2 = sC + 2*dir;
                if (dir < 0) { // largo: need one more square between (sC-1, sC-2, sC-3) where rook at 0
                    // for queen-side, ensure squares between sC-1, sC-2, sC-3 are free (but king moves only through sC-1 and sC-2)
                    for (int cc = rookCol+1; cc < sC; ++cc){
                        if (tableroLogico[sF][cc] != -1) return false;
                    }
                } else {
                    // short: check squares between sC+1 .. rookCol-1
                    for (int cc = sC+1; cc < rookCol; ++cc){
                        if (tableroLogico[sF][cc] != -1) return false;
                    }
                }
                // comprobar que el rey NO está en jaque actualmente y que las casillas por las que pasará (sC, sC+dir, sC+2dir)
                // no estén atacadas por el enemigo.
                ColorPieza enemigo = (p.color==ColorPieza::White)? ColorPieza::Black : ColorPieza::White;
                // casilla actual (sF,sC) no debe estar atacada
                if (estaCasillaAtacada(piezas, enemigo, sF, sC)) return false;
                // casilla intermedia y final
                int passC = sC + dir;
                int finalC = sC + 2*dir;
                if (estaCasillaAtacada(piezas, enemigo, sF, passC)) return false;
                if (estaCasillaAtacada(piezas, enemigo, sF, finalC)) return false;
                // si todo OK, permitir enroque
                return true;
            }

            return false;
        }
    }
    return false;
}

// Encuentra índice del rey de color 'color' en piezas, -1 si no existe
inline int encontrarIndiceRey(const std::vector<Pieza>& piezas, ColorPieza color){
    for(int i=0;i<(int)piezas.size();++i){
        if (piezas[i].alive && piezas[i].tipo==TipoPieza::King && piezas[i].color==color)
            return i;
    }
    return -1;
}

// Determina si 'color' está en jaque (false si no hay rey)
inline bool estaEnJaque(const std::vector<Pieza>& piezas, ColorPieza color){
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
inline bool dejaReyEnJaqueSimulado(std::vector<Pieza>& piezas, int moverIdx, int dstF, int dstC){
    // respaldos
    int backupTab[FILAS][COLS];
    for(int r=0;r<FILAS;r++) for(int c=0;c<COLS;c++) backupTab[r][c] = tableroLogico[r][c];
    std::vector<Pieza> backupPiezas = piezas;

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

    // manejar enroque en simulación: si mover es rey y se movió 2 casillas horizontales, mover la torre también (simulación)
    if (backupPiezas[moverIdx].tipo == TipoPieza::King && std::abs(dstC - srcC) == 2){
        int dir = (dstC - srcC) > 0 ? 1 : -1;
        int rookCol = (dir>0)? 7 : 0;
        int rookIdx = backupTab[srcF][rookCol];
        if (rookIdx != -1){
            int newRookCol = srcC + dir;
            // quitar torre antigua
            if (dentroTablero(srcF, rookCol)) tableroLogico[srcF][rookCol] = -1;
            // mover torre en simulacion
            tableroLogico[srcF][newRookCol] = rookIdx;
            piezas[rookIdx].fila = srcF;
            piezas[rookIdx].col = newRookCol;
        }
    }

    // encontrar rey del color del que movimos
    ColorPieza colorMover = piezas[moverIdx].color;
    bool enJaque = estaEnJaque(piezas, colorMover);

    // restaurar
    piezas = backupPiezas;
    for(int r=0;r<FILAS;r++) for(int c=0;c<COLS;c++) tableroLogico[r][c] = backupTab[r][c];

    return enJaque;
}

// Comprueba si el jugador 'color' está en jaque mate
inline bool esJaqueMate(std::vector<Pieza>& piezas, ColorPieza color){
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


