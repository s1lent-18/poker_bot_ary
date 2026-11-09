import React from "react";
import { BotOption, SessionStats } from "@/lib/types";
import { Volume2, VolumeX, RotateCcw, Cpu, History } from "lucide-react";

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
      {/* Brand & Bot Selection */}
      <div className="flex items-center gap-3">
        <div className="flex items-center gap-1.5 font-mono font-bold tracking-wider text-zinc-100">
          <span className="w-2.5 h-2.5 bg-emerald-500 rounded-sm inline-block" />
          <span>HOLD&apos;EM 1v1</span>
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

      {/* Session Telemetry */}
      <div className="flex items-center gap-3 text-xs font-mono">
        {/* Hold'em Fallback Metric */}
        {selectedBotId === "cfr" && (
          <div
            className="hidden md:flex items-center gap-1.5 bg-zinc-900 border border-zinc-800 rounded px-2.5 py-1 text-zinc-400"
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
      </div>

      {/* Right Controls: History Modal Toggle, 4-Color, Audio, Reset */}
      <div className="flex items-center gap-2">
        {/* Dedicated History Button */}
        <button
          onClick={onToggleHistory}
          className={`px-2.5 py-1 text-xs font-mono rounded border flex items-center gap-1.5 transition-colors cursor-pointer ${
            isHistoryOpen
              ? "bg-emerald-950 border-emerald-600 text-emerald-200"
              : "bg-zinc-900 border-zinc-750 text-zinc-300 hover:bg-zinc-850"
          }`}
          title="Open Hand History Log"
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
