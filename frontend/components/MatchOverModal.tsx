import React from "react";
import { Trophy, Skull, RotateCcw, Infinity as InfinityIcon } from "lucide-react";

interface MatchOverModalProps {
  isOpen: boolean;
  winner: "hero" | "bot" | string;
  botName: string;
  handsPlayed: number;
  heroBankroll: number;
  botBankroll: number;
  onPlayAgain: () => void;
  onPlayInfinite: () => void;
}

export const MatchOverModal: React.FC<MatchOverModalProps> = ({
  isOpen,
  winner,
  botName,
  handsPlayed,
  heroBankroll,
  botBankroll,
  onPlayAgain,
  onPlayInfinite,
}) => {
  if (!isOpen) return null;

  const isHeroWinner = winner === "hero";

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/85 backdrop-blur-md animate-in fade-in duration-300">
      <div className={`w-full max-w-md bg-zinc-950 border ${
        isHeroWinner ? "border-emerald-500/60 shadow-emerald-950/50" : "border-red-500/60 shadow-red-950/50"
      } rounded-2xl shadow-2xl overflow-hidden text-zinc-100 flex flex-col font-sans`}>
        {/* Header Banner */}
        <div className={`px-6 py-6 flex flex-col items-center justify-center text-center border-b ${
          isHeroWinner
            ? "bg-gradient-to-b from-emerald-900/60 to-zinc-950 border-emerald-700/50"
            : "bg-gradient-to-b from-red-950/60 to-zinc-950 border-red-700/50"
        }`}>
          <div className={`w-16 h-16 rounded-2xl flex items-center justify-center mb-3 border shadow-lg ${
            isHeroWinner
              ? "bg-emerald-500/20 border-emerald-400/50 text-emerald-400"
              : "bg-red-500/20 border-red-400/50 text-red-400"
          }`}>
            {isHeroWinner ? (
              <Trophy className="w-8 h-8" />
            ) : (
              <Skull className="w-8 h-8" />
            )}
          </div>

          <h2 className="text-2xl font-black font-mono tracking-wider">
            {isHeroWinner ? (
              <span className="text-emerald-300">MATCH WON!</span>
            ) : (
              <span className="text-red-400">MATCH LOST!</span>
            )}
          </h2>

          <p className="text-xs text-zinc-400 mt-1">
            {isHeroWinner
              ? `Congratulations! You eliminated ${botName} and conquered the match!`
              : `You were stacked by ${botName}. Better luck next match!`}
          </p>
        </div>

        {/* Match Summary Stats */}
        <div className="p-6 flex flex-col gap-4">
          <div className="bg-zinc-900 border border-zinc-800 rounded-xl p-4 flex flex-col gap-3 font-mono text-xs">
            <div className="text-[11px] font-bold text-zinc-400 uppercase tracking-wider border-b border-zinc-800 pb-2">
              Match Recap
            </div>

            <div className="flex items-center justify-between">
              <span className="text-zinc-400">Champion:</span>
              <span className={`font-bold ${isHeroWinner ? "text-emerald-400" : "text-red-400"}`}>
                {isHeroWinner ? "You (Hero)" : botName}
              </span>
            </div>

            <div className="flex items-center justify-between">
              <span className="text-zinc-400">Hands Played:</span>
              <span className="text-zinc-200 font-bold">{handsPlayed}</span>
            </div>

            <div className="flex items-center justify-between">
              <span className="text-zinc-400">Your Final Stack:</span>
              <span className={`font-bold ${heroBankroll > 0 ? "text-emerald-400" : "text-zinc-500"}`}>
                {heroBankroll.toLocaleString()} chips
              </span>
            </div>

            <div className="flex items-center justify-between">
              <span className="text-zinc-400">{botName}&apos;s Final Stack:</span>
              <span className={`font-bold ${botBankroll > 0 ? "text-amber-400" : "text-zinc-500"}`}>
                {botBankroll.toLocaleString()} chips
              </span>
            </div>
          </div>

          {/* Action Options */}
          <div className="flex flex-col gap-2.5 pt-1">
            <button
              onClick={onPlayAgain}
              className={`w-full py-3 rounded-xl font-mono font-bold text-xs uppercase tracking-wider flex items-center justify-center gap-2 transition-colors cursor-pointer shadow-md ${
                isHeroWinner
                  ? "bg-emerald-600 hover:bg-emerald-500 active:bg-emerald-700 text-zinc-100"
                  : "bg-red-600 hover:bg-red-500 active:bg-red-700 text-zinc-100"
              }`}
            >
              <RotateCcw className="w-4 h-4" />
              <span>Play Again</span>
            </button>

            <button
              onClick={onPlayInfinite}
              className="w-full py-2.5 rounded-xl border border-zinc-700 bg-zinc-900 hover:bg-zinc-850 active:bg-zinc-800 text-zinc-300 font-mono font-medium text-xs flex items-center justify-center gap-2 transition-colors cursor-pointer"
            >
              <InfinityIcon className="w-4 h-4 text-emerald-400" />
              <span>Play Infinite Mode</span>
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};
