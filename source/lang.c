// Prelude — Nintendo Switch homebrew for the Nextendo Network.
// Copyright (C) 2026 Nextendo Network
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU Affero General Public License as published by the Free
// Software Foundation, either version 3 of the License, or (at your option) any
// later version.
//
// You should have received a copy of the GNU Affero General Public License along
// with this program. If not, see <https://www.gnu.org/licenses/>.

// ============================================================
//  Prelude — tables de traductions EN / ES / PT.
//  La langue courante est lue/écrite depuis la SD.
// ============================================================
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <switch.h>

#include "lang.h"

Lang g_lang = LANG_EN;

#define LANG_PATH "sdmc:/switch/prelude_lang.txt"

// --- String tables -------------------------------------------------------
// Each column: EN, ES, PT, FR.
// Must match StringID enum order exactly.
// Brand names (Nextendo, Nintendo, Prelude) are kept as-is.
static const char *s_strings[STR_COUNT][5] = {
    // --- Picker screen ---
    [STR_TITLE_PRELUDE]      = { "Prelude", "Prelude", "Prelude", "Prelude", "プレールード" },
    [STR_MODE_ACTUAL_PREFIX] = { "Current mode: ", "Modo actual: ", "Modo atual: ", "Mode actuel : ", "現在のモード: " },
    [STR_CURRENT_BADGE]      = { "CURRENT", "ACTUAL", "ATUAL", "ACTUEL", "現在" },
    [STR_SWITCH_BADGE]       = { "SWITCH HERE", "CAMBIAR AQUI", "MUDAR AQUI", "BASICULER ICI", "こちら変更" },

    // --- Context descriptions ---
    [STR_DESC_NEXTENDO]      = { "Nextendo servers: custom online, icons, friends. Nextendo account required.",
                                 "Servidores Nextendo: online custom, iconos, amigos. Cuenta Nextendo requerida.",
                                 "Servidores Nextendo: online custom, iconos, amigos. Conta Nextendo necessaria.",
                                   "Serveurs Nextendo : online custom, icônes, amis. Compte Nextendo requis.",
                                   "NEXTENDOサーバー: カスタムオンライン、アイコン、フレンド。NEXTENDOのアカウントは必要です。" },
    [STR_DESC_NINTENDO]      = { "Official Nintendo servers: normal console operation.",
                                 "Servidores oficiales Nintendo: funcionamiento normal de la consola.",
                                 "Servidores oficiais Nintendo: funcionamento normal do console.",
                                 "Serveurs officiels Nintendo : fonctionnement normal de la console.",
                                   "公式NINTENDOサーバー: 通常のコンソール動作。" }
                                   },
    [STR_DESC_S2]            = { "Downloads the Splatoon 2 rotation schedule and installs it into the game.",
                                 "Descarga el calendario de modos de Splatoon 2 y lo instala en el juego.",
                                 "Baixa o calendario de modos do Splatoon 2 e instala no jogo.",
                                  "Télécharge le calendrier des modes de Splatoon 2 et l'installe dans le jeu.",
                                  "スプラトゥーン２のローテーションスケジュールをダウンロードしゲームにインストールする。" },

    // --- Help lines ---
    [STR_HELP_MODE]          = { "A: switch mode       < >: change choice       B: quit",
                                 "A: cambiar modo       < >: cambiar eleccion       B: salir",
                                 "A: mudar modo         < >: mudar escolha          B: sair",
                                  "A : changer de mode       < > : changer de choix       B : quitter",
                                  "A: モード切替       < >: 選択変更       B: 終了" },
    [STR_HELP_S2]            = { "A: install schedule       < >: change choice       B: quit",
                                 "A: instalar horario       < >: cambiar eleccion       B: salir",
                                 "A: instalar cronograma    < >: mudar escolha          B: sair",
                                  "A : installer le planning       < > : changer de choix       B : quitter",
                                  "A: スケジュールインストール       < >: 選択変更       B: 終了" },

    // --- Update banner ---
    [STR_UPDATE_BANNER]      = { "MANDATORY update (v%d)   -   press Y to install",
                                 "Actualizacion OBLIGATORIA (v%d)   -   presiona Y para instalar",
                                 "Atualizacao OBRIGATORIA (v%d)   -   pressione Y para instalar",
                                  "Mise à jour OBLIGATOIRE (v%d)   -   appuie sur Y pour installer",
                                  "必須アップデート (v%d)   -   Yボタンでインストール" },

    // --- Confirm screen ---
    [STR_CONFIRM_NEXTENDO]   = { "Switch to NEXTENDO mode?",
                                 "Pasar a modo NEXTENDO?",
                                 "Mudar para o modo NEXTENDO?",
                                  "Passer en mode NEXTENDO ?",
                                  "NEXTENDOモードに切り替えますか？" },
    [STR_CONFIRM_NINTENDO]   = { "Switch to NINTENDO mode?",
                                 "Pasar a modo NINTENDO?",
                                 "Mudar para o modo NINTENDO?",
                                  "Passer en mode NINTENDO ?",
                                  "NINTENDOモードに切り替えますか？" },
    [STR_CONFIRM_REBOOT]     = { "The console will REBOOT now.",
                                 "La consola va a REINICIAR ahora.",
                                 "O console vai REINICIAR agora.",
                                  "La console va REDÉMARRER maintenant.",
                                  "コンソールは今すぐ再起動する。" },
    [STR_CONFIRM_RESTART_NEXTENDO] = { "After reboot: connected to Nextendo Network servers.",
                                       "Al reiniciar: conectado a los servidores Nextendo Network.",
                                       "Ao reiniciar: conectado aos servidores Nextendo Network.",
                                       "Au redémarrage : connecté aux serveurs Nextendo Network.",
                                       "再起動後: NEXTENDO NETWORKサーバーに接続する。"},
    [STR_CONFIRM_RESTART_NINTENDO] = { "After reboot: back to official Nintendo servers.",
                                       "Al reiniciar: vuelta a los servidores oficiales Nintendo.",
                                       "Ao reiniciar: volta aos servidores oficiais Nintendo.",
                                       "Au redémarrage : retour aux serveurs officiels Nintendo.",
                                       "再起動後: 公式NINTENDOサーバーに接続する。"},
    [STR_CONFIRM_CLOSE_GAMES] = { "Close any running games before confirming.",
                                  "Cierra cualquier juego abierto antes de confirmar.",
                                  "Feche qualquer jogo aberto antes de confirmar.",
                                   "Ferme tout jeu en cours avant de confirmer.",
                                   "確認する前に、実行中のゲームを閉じてください。" },
    [STR_CONFIRM_A]          = { "A: Confirm", 
                                 "A: Confirmar", 
                                 "A: Confirmar", 
                                 "A : Confirmer", 
                                 "A: 確認" },
    [STR_CONFIRM_B]          = { "B: Cancel", 
                                 "B: Cancelar", 
                                 "B: Cancelar", 
                                 "B : Annuler", 
                                 "B: キャンセル" },

    // --- No-emuMMC warnings ---
    [STR_WARN_TITLE]         = { "WARNING: this console has no emuMMC.",
                                 "ATENCION: esta consola no tiene emuMMC.",
                                 "ATENCAO: este console nao tem emuMMC.",
                                  "ATTENTION : cette console n'a pas d'emuMMC.",
                                  "警告: このコンソールにはemuMMCありません。" },
    [STR_WARN_LINE1]         = { "CFW runs on internal memory with your real console ID.",
                                 "El CFW funciona en la memoria interna con tu identificador real.",
                                 "O CFW roda na memoria interna com seu ID real do console.",
                                 "Le CFW tourne sur la mémoire interne avec ton vrai identifiant console.",
                                 "CFWは、実際のコンソールIDを使用して内部メモリで実行されます。" },
    [STR_WARN_LINE2]         = { "No identity protection without breaking eShop.",
                                 "No es posible proteger la identidad sin romper eShop.",
                                 "Nao e possivel proteger a identidade sem quebrar o eShop.",
                                 "Aucune protection d'identité possible sans casser l'eShop.",
                                 "eShopを壊さずにID保護はできません。" },
    [STR_WARN_LINE3]         = { "Going ONLINE on real Nintendo remains at your own risk.",
                                 "Ir EN LINEA al Nintendo real sigue siendo bajo tu riesgo.",
                                 "Ir ONLINE no Nintendo real permanece sob seu risco.",
                                 "Aller EN LIGNE sur le vrai Nintendo reste à tes risques.",
                                 "実際のNINTENDOでオンラインに接続することは、自己責任です。" },
    [STR_WARN_LINE4]         = { "(Telemetry is still blocked.)",
                                 "(La telemetria sigue bloqueada.)",
                                 "(A telemetria continua bloqueada.)",
                                 "(La télémétrie reste bloquée.)",
                                   "(テレメトリはまだブロックされています。)" },

    // --- S2 bar (picker screen) ---
    [STR_S2_BAR]             = { "Splatoon 2   -   Install online schedule",
                                 "Splatoon 2   -   Instalar planning en linea",
                                 "Splatoon 2   -   Instalar cronograma online",
                                 "Splatoon 2   -   Installer le planning en ligne",
                                    "スプラトゥーン２   -   オンラインスケジュールをインストール" },

    // --- S2 info screen ---
    [STR_S2_TITLE]           = { "Splatoon 2 online schedule",
                                 "Planning en linea Splatoon 2",
                                 "Cronograma online Splatoon 2",
                                 "Planning en ligne Splatoon 2",
                                  "スプラトゥーン２オンラインスケジュール" },
    [STR_S2_DESC1]           = { "Downloads the rotation schedule (Turf War, Salmon Run, Festivals)",
                                 "Descarga el calendario de modos (Guerra Territorial, Salmon Run, Festivales)",
                                 "Baixa o calendario de modos (Guerra Territorial, Salmon Run, Festivais)",
                                 "Télécharge le calendrier des modes (Guerre Territoriale, Salmon Run, Festivals)",
                                    "ローテーションスケジュールをダウンロードする（ナワバリバトル、サーモンラン、フェス）" },
    [STR_S2_DESC2]           = { "from the Nextendo servers and installs it into Splatoon 2.",
                                 "desde los servidores Nextendo y lo instala en Splatoon 2.",
                                 "dos servidores Nextendo e instala no Splatoon 2.",
                                 "depuis les serveurs Nextendo et l'installe dans Splatoon 2.",
                                   "NEXTENDOサーバーからスプラトゥーン２にインストールする。" },
    [STR_S2_DESC3]           = { "Launch Splatoon 2 at least once first. No reboot required.",
                                 "Inicia Splatoon 2 al menos una vez antes. No requiere reinicio.",
                                 "Inicie Splatoon 2 pelo menos uma vez antes. Nao requer reinicializacao.",
                                 "Lance Splatoon 2 au moins une fois avant. Aucun redémarrage requis.",
                                    "スプラトゥーン２を少なくとも一度起動してください。再起動は不要です。" },
    [STR_S2_DESC4]           = { "Each installation REPLACES the previous schedule.",
                                 "Cada instalacion REEMPLAZA el planning anterior.",
                                 "Cada instalacao SUBSTITUI o cronograma anterior.",
                                 "Chaque installation REMPLACE le planning précédent.",
                                    "各インストールは前のスケジュールを置き換えます。" },
    [STR_S2_A]               = { "A: Install", "A: Instalar", "A: Instalar", "A : Installer", "A : インストール" },
    [STR_S2_B]               = { "B: Back", "B: Volver", "B: Voltar", "B : Retour", "B : 戻る" },

    // --- Progress screen ---
    [STR_PROGRESS_TITLE]     = { "Installation", "Instalacion", "Instalacao", "Installation", "インストール" },
    [STR_PROGRESS_WAIT]      = { "Please wait...", "Espere por favor...", "Aguarde por favor...", "Patiente...", "お待ちください..." },

    // --- Result screen ---
    [STR_RESULT_RETURN]      = { "A / B: Return", "A / B: Volver", "A / B: Voltar", "A / B : Retour", "A / B : 戻る" },

    // --- Status messages (main.c) ---
    [STR_STATUS_NEXTENDO_OK]   = { "NEXTENDO mode applied  -  rebooting...",
                                    "Modo NEXTENDO aplicado  -  reiniciando...",
                                    "Modo NEXTENDO aplicado  -  reiniciando...",
                                    "Mode NEXTENDO appliqué  -  redémarrage...",
                                    "NEXTENDOモードが適用されました  -  再起動中..." },
    [STR_STATUS_NINTENDO_OK]   = { "NINTENDO mode applied  -  rebooting...",
                                   "Modo NINTENDO aplicado  -  reiniciando...",
                                   "Modo NINTENDO aplicado  -  reiniciando...",
                                   "Mode NINTENDO appliqué  -  redémarrage..." ,
                                   "NINTENDOモードが適用されました  -  再起動中..." },
    [STR_STATUS_REBOOT_FAIL]   = { "Reboot failed",
                                   "Error al reiniciar",
                                   "Falha ao reiniciar",
                                   "Échec du redémarrage",
                                   "再起動に失敗しました" },
    [STR_STATUS_SD_ERROR]      = { "ERROR: cannot write to SD card",
                                   "ERROR: no se puede escribir en la SD",
                                   "ERRO: nao e possivel escrever no cartao SD",
                                   "ERREUR : écriture sur la SD impossible",
                                   "エラー: SDカードに書き込めません" },
    [STR_STATUS_DOWNLOAD_SCHEDULE] = { "Downloading and installing schedule...",
                                       "Descargando e instalando planning...",
                                       "Baixando e instalando cronograma...",
                                   "Téléchargement et installation du planning...",
                                   "スケジュールをダウンロードしてインストール中..." },
    [STR_STATUS_SCHEDULE_OK]   = { "Schedule installed",
                                   "Planning instalado",
                                   "Cronograma instalado",
                                   "Planning installé",
                                   "スケジュールがインストールされました" },
    [STR_STATUS_SCHEDULE_OK_DESC]  = { "Old schedule replaced. Relaunch Splatoon 2 to apply.",
                                       "Planning anterior reemplazado. Reinicia Splatoon 2 para aplicar.",
                                       "Cronograma anterior substituido. Reinicie Splatoon 2 para aplicar.",
                                   "Ancien planning remplacé. Relance Splatoon 2 pour l'appliquer.",
                                   "古いスケジュールが置き換えられました。スプラトゥーン２を再起動して適用してください。" },
    [STR_STATUS_NO_SCHEDULE]   = { "No schedule",
                                   "Sin planning",
                                   "Sem cronograma",
                                   "Aucun planning",
                                   "スケジュールなし" },
    [STR_STATUS_NO_SCHEDULE_DESC]  = { "Nothing published yet.",
                                       "Nada publicado aun.",
                                       "Nada publicado ainda.",
                                   "Rien de publié pour le moment.",
                                   "まだ公開されていません。" },
    [STR_STATUS_MOUNT_FAIL]    = { "Save data not found",
                                   "Datos de guardado no encontrados",
                                   "Dados salvos nao encontrados",
                                   "Sauvegarde introuvable",
                                   "セーブデータが見つかりません" },
    [STR_STATUS_MOUNT_FAIL_DESC]   = { "Launch Splatoon 2 once, then try again.",
                                       "Inicia Splatoon 2 una vez, luego intenta de nuevo.",
                                       "Inicie Splatoon 2 uma vez, entao tente novamente.",
                                   "Lance Splatoon 2 une fois, puis réessaie.",
                                   "スプラトゥーン２を一度起動して、再試行してください。" },
    [STR_STATUS_NET_FAIL]      = { "Network error",
                                   "Error de red",
                                   "Erro de rede",
                                   "Erreur réseau",
                                   "ネットワークエラー" },
    [STR_STATUS_NET_FAIL_DESC] = { "Download failed (check connection / mode).",
                                   "Fallo la descarga (verifica conexion / modo).",
                                   "Falha no download (verifique conexao / modo).",
                                   "Téléchargement impossible (vérifie la connexion / le mode).",
                                   "ダウンロードに失敗しました（接続/モードを確認してください）。" },
    [STR_STATUS_WRITE_FAIL]    = { "Write error",
                                   "Error de escritura",
                                   "Erro de gravacao",
                                   "Erreur d'écriture",
                                   "書き込みエラー" },
    [STR_STATUS_WRITE_FAIL_DESC]   = { "Cannot write to save data.",
                                       "No se puede escribir en los datos de guardado.",
                                       "Nao e possivel gravar nos dados salvos.",
                                   "Écriture dans la sauvegarde impossible.",
                                   "セーブデータに書き込めません。" },
    [STR_STATUS_NET_CONNECT]   = { "Server unreachable",
                                   "Servidor inaccesible",
                                   "Servidor inacessivel",
                                   "Serveur inaccessible" 
                                   "サーバーに接続できません" },
    [STR_STATUS_NET_CONNECT_DESC] = { "Cannot connect to server (check internet / mode).",
                                      "No se puede conectar al servidor (verifica internet / modo).",
                                      "Nao e possivel conectar ao servidor (verifique internet / modo).",
                                   "Impossible de contacter le serveur (vérifie connexion / mode).",
                                   "サーバー接続は失敗しました（インターネット/モードを確認してください）。" },
    [STR_STATUS_NET_TIMEOUT]   = { "Connection timeout",
                                   "Tiempo de conexion agotado",
                                   "Tempo de conexao esgotado",
                                   "Délai de connexion dépassé",
                                   "接続タイムアウト" },
    [STR_STATUS_NET_TIMEOUT_DESC] = { "Server took too long to respond. Try again.",
                                      "El servidor tardo demasiado en responder. Intenta de nuevo.",
                                      "O servidor demorou muito para responder. Tente novamente.",
                                   "Le serveur a mis trop de temps à répondre. Réessaie.",
                                   "サーバー応答時間遅すぎました。再試行してください。" },
    [STR_STATUS_NET_HTTP_ERR]  = { "Server error",
                                   "Error del servidor",
                                   "Erro do servidor",
                                   "Erreur du serveur",
                                   "サーバーエラー"　},
    [STR_STATUS_NET_HTTP_ERR_DESC] = { "Server returned an unexpected response.",
                                       "El servidor devolvio una respuesta inesperada.",
                                       "O servidor retornou uma resposta inesperada.",
                                   "Le serveur a renvoyé une réponse inattendue.",
                                   "サーバーが予期しない応答を返しました。" },
    [STR_STATUS_DOWNLOAD_UPDATE]   = { "Downloading update...",
                                       "Descargando actualizacion...",
                                       "Baixando atualizacao...",
                                   "Téléchargement de la mise à jour..." ,
                                   "アップデートダウンロード中..." },
    [STR_STATUS_UPDATE_OK]     = { "Update installed",
                                   "Actualizacion instalada",
                                   "Atualizacao instalada",
                                   "Mise à jour installée",
                                   "アップデートがインストールされました" },
    [STR_STATUS_UPDATE_OK_DESC]    = { "CLOSE and relaunch Prelude to apply v%d.",
                                        "CIERRA y reinicia Prelude para aplicar v%d.",
                                        "FECHE e reinicie o Prelude para aplicar v%d.",
                                   "FERME et relance Prelude pour appliquer la v%d." 
                                   "プレールードを閉じて再起動し、v%dを適用してください。" },
    [STR_STATUS_UPDATE_SIZE_FAIL]  = { "Download corrupted",
                                       "Descarga corrupta",
                                       "Download corrompido",
                                   "Téléchargement corrompu",
                                   "ダウンロード破損。" },
    [STR_STATUS_UPDATE_SIZE_FAIL_DESC] = { "Unexpected size. Try again.",
                                           "Tamano inesperado. Intenta de nuevo.",
                                           "Tamanho inesperado. Tente novamente.",
                                   "Taille inattendue. Réessaie.",
                                   "予想外サイズ。再試行してください。" },
    [STR_STATUS_UPDATE_WRITE_FAIL] = { "Write failed",
                                       "Error de escritura",
                                       "Falha ao escrever",
                                   "Échec d'écriture"
                                   "読み込むエラー" },
    [STR_STATUS_UPDATE_WRITE_FAIL_DESC] = { "Cannot write to SD card.",
                                            "No se puede escribir en la SD.",
                                            "Nao e possivel escrever no cartao SD.",
                                   "Impossible d'écrire sur la carte SD.",
                                   "SDカードに書き込めません。" },
    [STR_STATUS_UPDATE_NET_FAIL]   = { "Network error",
                                       "Error de red",
                                       "Erro de rede",
                                   "Erreur réseau",
                                   "ネットワークエラー" },
    [STR_STATUS_UPDATE_NET_FAIL_DESC] = { "Download failed.",
                                          "Fallo la descarga.",
                                          "Falha no download.",
                                   "Téléchargement impossible.",
                                   "ダウンロードに失敗しました。" },

    // --- Language menu ---
    [STR_LANG_TITLE]        = { "Language", "Idioma", "Idioma", "Langue", "言語" },
    [STR_LANG_EN]           = { "English", "Ingles", "Ingles", "Anglais", "英語" },
    [STR_LANG_ES]           = { "Spanish", "Espanol", "Espanhol", "Espagnol", "スペイン語" },
    [STR_LANG_PT]           = { "Portuguese", "Portugues", "Portugues", "Portuguais", "ポルトガル語" },
    [STR_LANG_FR]           = { "French", "Frances", "Frances", "Francais", "フランス語" },
    [STR_LANG_DEFAULT]      = { "(Default)", "(Defecto)", "(Padrao)", "(Défaut)", "(デフォルト)" },
    [STR_LANG_A_SELECT]     = { "A: Select", "A: Elegir", "A: Selecionar", "A : Choisir", "A: 選択" },
    [STR_LANG_B_BACK]       = { "B: Back", "B: Volver", "B: Voltar", "B : Retour", "B: 戻る" },

    // --- Update confirmation ---
    [STR_UPD_CONFIRM_TITLE]    = { "Update available",
                                   "Actualizacion disponible",
                                   "Atualizacao disponivel",
                                   "Mise à jour disponible",
                                   "アップデートは利用可能です" },
    [STR_UPD_CONFIRM_VERSION]  = { "New version: build %d",
                                   "Nueva version: build %d",
                                   "Nova versao: build %d",
                                   "Nouvelle version : build %d",
                                   "新しいバージョン: ビルド %d" },
    [STR_UPD_CONFIRM_DESC]    = { "Prelude will download and replace itself.",
                                  "Prelude descargara y se reemplazara a si mismo.",
                                  "Prelude baixara e substituira a si mesmo.",
                                  "Prelude va se télécharger et se remplacer.",
                                  "プレールードはダウンロードされて自身を置き換えます。" },
    [STR_UPD_CONFIRM_A]       = { "A: Download and install",
                                  "A: Descargar e instalar",
                                  "A: Baixar e instalar",
                                  "A: Télécharger et installer",
                                  "A: ダウンロードしてインストール" },
    [STR_UPD_CONFIRM_B]       = { "B: Cancel",
                                  "B: Cancelar",
                                  "B: Cancelar",
                                  "B: Annuler",
                                  "B: キャンセル" },
};

// ============================================================

void lang_init(void) {
    FILE *f = fopen(LANG_PATH, "rb");
    if (!f) { g_lang = LANG_EN; return; }
    int c = fgetc(f);
    fclose(f);
    if (c == 'E') g_lang = LANG_EN;
    else if (c == 'S') g_lang = LANG_ES;
    else if (c == 'P') g_lang = LANG_PT;
    else if (c == 'F') g_lang = LANG_FR;
    else g_lang = LANG_EN;
}

void lang_save(void) {
    char ch = 'E';
    if (g_lang == LANG_ES) ch = 'S';
    else if (g_lang == LANG_PT) ch = 'P';
    else if (g_lang == LANG_FR) ch = 'F';
    // else LANG_EN -> 'E'
    FILE *f = fopen(LANG_PATH, "wb");
    if (!f) {
        mkdir("sdmc:/switch", 0777);
        f = fopen(LANG_PATH, "wb");
    }
    if (f) { fputc(ch, f); fclose(f); }
    fsdevCommitDevice("sdmc");
}

const char *lang_str(StringID id) {
    if (id < 0 || id >= STR_COUNT) return "?";
    if (g_lang < 0 || g_lang >= (int)(sizeof(s_strings[0]) / sizeof(s_strings[0][0])))
        g_lang = LANG_EN;
    return s_strings[id][g_lang];
}
