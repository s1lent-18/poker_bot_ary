import React from "react";
import { BotOption, SessionStats, MatchInfo } from "@/lib/types";
import { Volume2, VolumeX, RotateCcw, Cpu, History, Eye, EyeOff, Coins, Trophy, Infinity as InfinityIcon } from "lucide-react";

interface HeaderBarProps {
  bots: BotOption[];
  selectedBotId: string;
  selectedVersion: string;
  onSelectBot: (botId: string) => void;
  onSelectVersion: (version: string) => void;
  session: SessionStats;
  isMuted: boolean;
  onToggleMute: () => void;
  fourColor: boolean;
  onToggleFourColor: () => void;
  onResetSession: () => void;
  isPending: boolean;
  historyCount: number;
  onToggleHistory: () => void;
  isHistoryOpen: boolean;
  botCheckEnabled: boolean;
  onToggleBotCheck: () => void;
  autoRebuy: boolean;
  onToggleAutoRebuy: () => void;
  gameMode: "infinite" | "match";
  onSelectGameMode: (mode: "infinite" | "match") => void;
  matchInfo?: MatchInfo;
}

export const HeaderBar: React.FC<HeaderBarProps> = ({
  bots,
  selectedBotId,
  selectedVersion,
  onSelectBot,
  onSelectVersion,
  session,
  isMuted,
  onToggleMute,
  fourColor,
  onToggleFourColor,
  onResetSession,
  isPending,
  historyCount,
  onToggleHistory,
  isHistoryOpen,
  botCheckEnabled,
  onToggleBotCheck,
  autoRebuy,
  onToggleAutoRebuy,
  gameMode,
  onSelectGameMode,
  matchInfo,
}) => {
  const currentBot = bots.find((b) => b.id === selectedBotId);
  const versions = currentBot?.versions || [];

  const formatWinRate = (val: number) => {
    const sign = val > 0 ? "+" : "";
    return `${sign}${val.toFixed(1)} bb/h`;
  };

  const formatNetChips = (val: number) => {
    const sign = val > 0 ? "+" : "";
    return `${sign}${val.toLocaleString()}`;
  };

  return (
    <header className="w-full bg-zinc-950 border-b border-zinc-800 px-3 sm:px-6 py-2.5 flex flex-wrap items-center justify-between gap-3 text-zinc-300 text-xs sm:text-sm select-none">
      {/* Brand & Bot Selection & Mode Switcher */}
      <div className="flex items-center flex-wrap gap-2.5 sm:gap-3">
        <div className="flex items-center gap-1.5 font-mono font-bold tracking-wider text-zinc-100">
          <span className="w-2.5 h-2.5 bg-emerald-500 rounded-sm inline-block" />
          <span>HOLD&apos;EM 1v1</span>
        </div>

        {/* Game Mode Switcher Tab */}
        <div className="flex items-center bg-zinc-900 border border-zinc-750 rounded-lg p-0.5">
          <button
            onClick={() => onSelectGameMode("infinite")}
            disabled={isPending}
            className={`px-2.5 py-1 rounded text-xs font-mono font-medium transition-all flex items-center gap-1.5 cursor-pointer ${
              gameMode === "infinite"
                ? "bg-emerald-600 text-zinc-100 font-bold shadow-sm"
                : "text-zinc-400 hover:text-zinc-200"
            }`}
            title="Infinite Mode: continuous poker with standard blinds"
          >
            <InfinityIcon className="w-3.5 h-3.5" />
            <span>Infinite</span>
          </button>
          <button
            onClick={() => onSelectGameMode("match")}
            disabled={isPending}
            className={`px-2.5 py-1 rounded text-xs font-mono font-medium transition-all flex items-center gap-1.5 cursor-pointer ${
              gameMode === "match"
                ? "bg-amber-600 text-zinc-100 font-bold shadow-sm"
                : "text-zinc-400 hover:text-zinc-200"
            }`}
            title="Match Mode: 10,000 chip elimination duel with escalating blinds"
          >
            <Trophy className="w-3.5 h-3.5 text-amber-300" />
            <span>Match Mode</span>
          </button>
        </div>

        {/* Bot selector */}
        <div className="flex items-center gap-1.5 bg-zinc-900 border border-zinc-700 rounded px-2.5 py-1">
          <Cpu className="w-3.5 h-3.5 text-zinc-400" />
          <select
            value={selectedBotId}
            onChange={(e) => onSelectBot(e.target.value)}
            disabled={isPending}
            className="bg-transparent text-zinc-200 text-xs font-semibold focus:outline-none cursor-pointer"
          >
            {bots.map((b) => (
              <option key={b.id} value={b.id} className="bg-zinc-900 text-zinc-200">
                {b.name}
              </option>
            ))}
          </select>

          {/* Hold'em CFR version selector */}
          {selectedBotId === "cfr" && versions.length > 0 && (
            <select
              value={selectedVersion}
              onChange={(e) => onSelectVersion(e.target.value)}
              disabled={isPending}
              aria-label="Hold'em CFR checkpoint version"
              className="bg-zinc-800 text-emerald-400 text-xs font-mono font-medium rounded px-2 py-0.5 border border-zinc-700 focus:outline-none cursor-pointer ml-1"
            >
              {versions.map((v) => (
                <option key={v.id} value={v.id} className="bg-zinc-900 text-zinc-200">
                  {v.filename} {v.iterations > 0 ? `(${Math.round(v.iterations / 1000)}k iters)` : ""}
                </option>
              ))}
            </select>
          )}
        </div>
      </div>

      {/* Center Match Banner / Telemetry */}
      <div className="flex items-center gap-3 text-xs font-mono">
        {gameMode === "match" && matchInfo ? (
          <div className="flex items-center gap-2 bg-amber-950/40 border border-amber-600/60 rounded px-3 py-1 text-amber-200">
            <Trophy className="w-3.5 h-3.5 text-amber-400" />
            <span className="font-bold text-amber-300">
              Hand #{matchInfo.matchHandNumber}
            </span>
            <span className="text-zinc-600">|</span>
            <span className="text-zinc-300">
              Blinds: <strong className="text-amber-300">{matchInfo.sb}/{matchInfo.bb}</strong>
            </span>
            {matchInfo.nextIncreaseHand > 0 && (
              <span className="hidden md:inline text-[11px] text-zinc-400">
                (Up to {matchInfo.nextSb}/{matchInfo.nextBb} at Hand {matchInfo.nextIncreaseHand})
              </span>
            )}
          </div>
        ) : (
          <>
            {/* Hold'em Fallback Metric */}
            {selectedBotId === "cfr" && (
              <div
                className="hidden lg:flex items-center gap-1.5 bg-zinc-900 border border-zinc-800 rounded px-2.5 py-1 text-zinc-400"
                title="Info sets training never reached. Bot falls back to check/call."
              >
                <span className="text-zinc-500">Hold&apos;em Fallbacks:</span>
                <span
                  className={`font-semibold ${
                    session.holdenFallbacks > 0 ? "text-amber-400" : "text-emerald-400"
                  }`}
                >
                  {session.holdenFallbacks} / {session.holdenDecisions} ({session.holdenFallbackPct.toFixed(1)}%)
                </span>
              </div>
            )}

            {/* Bankroll */}
            <div className="flex items-center gap-2 bg-zinc-900 border border-zinc-800 rounded px-2.5 py-1">
              <span className="text-zinc-400 font-sans">Bankroll:</span>
              <span
                className={`font-semibold ${
                  session.totalChipsWon > 0
                    ? "text-emerald-400"
                    : session.totalChipsWon < 0
                    ? "text-red-400"
                    : "text-zinc-300"
                }`}
              >
                {formatNetChips(session.totalChipsWon)} chips
              </span>
              <span className="text-zinc-600">|</span>
              <span className="text-zinc-400">{formatWinRate(session.bbPerHand)}</span>
              <span className="text-zinc-500">({session.handsPlayed} h)</span>
            </div>
          </>
        )}
      </div>

      {/* Right Controls: Stack Mode Toggle, BotCheck, History, 4-Color, Audio, Reset */}
      <div className="flex items-center gap-2">
        {/* Stack Mode Button (disabled / hidden or elimination badge in Match Mode) */}
        {gameMode === "match" ? (
          <div
            className="px-2.5 py-1 text-xs font-mono rounded border border-amber-800/80 bg-amber-950/40 text-amber-300 flex items-center gap-1.5"
            title="Match Mode: 10k elimination tournament. No rebuys."
          >
            <Coins className="w-3.5 h-3.5 text-amber-400" />
            <span className="hidden sm:inline">Stack:</span>
            <span className="font-bold">Elimination</span>
          </div>
        ) : (
          <button
            onClick={onToggleAutoRebuy}
            className={`px-2.5 py-1 text-xs font-mono rounded border flex items-center gap-1.5 transition-colors cursor-pointer ${
              autoRebuy
                ? "bg-zinc-850 border-zinc-700 text-zinc-300"
                : "bg-emerald-950/80 border-emerald-600 text-emerald-200"
            }`}
            title={
              autoRebuy
                ? "Auto Reset 10k: Stacks reset to 10,000 every hand. Click to switch to Persistent."
                : "Persistent Stacks: Stacks carry over until bust (<100 chips), then rebuys 10,000 chips. Click to switch to Reset."
            }
          >
            <Coins className="w-3.5 h-3.5" />
            <span className="hidden sm:inline">Stack:</span>
            <span className={`font-bold ${autoRebuy ? "text-zinc-400" : "text-emerald-300"}`}>
              {autoRebuy ? "Reset 10k" : "Persistent"}
            </span>
          </button>
        )}

        {/* BotCheck Mode Toggle */}
        <button
          onClick={onToggleBotCheck}
          className={`px-2.5 py-1 text-xs font-mono rounded border flex items-center gap-1.5 transition-colors cursor-pointer ${
            botCheckEnabled
              ? "bg-amber-950/80 border-amber-600/90 text-amber-200"
              : "bg-zinc-900 border-zinc-750 text-zinc-400 hover:text-zinc-200"
          }`}
          title="BotCheck Mode: reveal bot hole cards in side window whenever bot or you fold"
        >
          {botCheckEnabled ? (
            <Eye className="w-3.5 h-3.5 text-amber-400" />
          ) : (
            <EyeOff className="w-3.5 h-3.5 text-zinc-500" />
          )}
          <span className="hidden sm:inline">BotCheck:</span>
          <span className={`font-bold ${botCheckEnabled ? "text-amber-300" : "text-zinc-500"}`}>
            {botCheckEnabled ? "ON" : "OFF"}
          </span>
        </button>

        {/* Dedicated History Button */}
        <button
          onClick={onToggleHistory}
          className={`px-2.5 py-1 text-xs font-mono rounded border flex items-center gap-1.5 transition-colors cursor-pointer ${
            isHistoryOpen
              ? "bg-emerald-950 border-emerald-600 text-emerald-200"
              : "bg-zinc-900 border-zinc-750 text-zinc-300 hover:bg-zinc-850"
          }`}
          title="Open Hand History Log (H)"
        >
          <History className="w-3.5 h-3.5" />
          <span>History</span>
          {historyCount > 0 && (
            <span className="bg-zinc-800 text-zinc-400 text-[10px] px-1.5 py-0.2 rounded font-mono">
              {historyCount}
            </span>
          )}
        </button>

        {/* 4-Color Deck Toggle */}
        <button
          onClick={onToggleFourColor}
          title="Toggle 4-color suit deck"
          className={`px-2 py-1 text-xs font-mono rounded border transition-colors cursor-pointer ${
            fourColor
              ? "bg-zinc-800 border-zinc-600 text-zinc-100"
              : "bg-transparent border-zinc-800 text-zinc-500 hover:text-zinc-300"
          }`}
        >
          4-Color
        </button>

        {/* Mute audio */}
        <button
          onClick={onToggleMute}
          title={isMuted ? "Unmute audio" : "Mute audio"}
          aria-label={isMuted ? "Unmute audio" : "Mute audio"}
          className="p-1.5 rounded border border-zinc-800 bg-zinc-900 text-zinc-400 hover:text-zinc-200 transition-colors cursor-pointer"
        >
          {isMuted ? <VolumeX className="w-4 h-4" /> : <Volume2 className="w-4 h-4" />}
        </button>

        {/* Reset Session */}
        <button
          onClick={onResetSession}
          title="Reset match & session stats"
          aria-label="Reset match and session stats"
          className="p-1.5 rounded border border-zinc-800 bg-zinc-900 text-zinc-400 hover:text-zinc-200 hover:border-zinc-700 transition-colors cursor-pointer"
        >
          <RotateCcw className="w-4 h-4" />
        </button>
      </div>
    </header>
  );
};
