import React from "react";
import { Trophy, Swords, ShieldAlert, ArrowRight, Zap } from "lucide-react";

interface MatchReadyModalProps {
  isOpen: boolean;
  botName: string;
  onConfirmStart: () => void;
  onCancelToInfinite: () => void;
}

export const MatchReadyModal: React.FC<MatchReadyModalProps> = ({
  isOpen,
  botName,
  onConfirmStart,
  onCancelToInfinite,
}) => {
  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center p-4 bg-black/80 backdrop-blur-sm animate-in fade-in duration-200">
      <div className="w-full max-w-lg bg-zinc-950 border border-emerald-500/40 rounded-2xl shadow-2xl overflow-hidden text-zinc-100 flex flex-col font-sans">
        {/* Header Ribbon */}
        <div className="bg-gradient-to-r from-emerald-950 via-zinc-900 to-emerald-950 border-b border-emerald-700/50 px-6 py-4 flex items-center gap-3">
          <div className="w-10 h-10 rounded-xl bg-emerald-500/20 border border-emerald-500/40 flex items-center justify-center text-emerald-400">
            <Trophy className="w-5 h-5" />
          </div>
          <div>
            <h2 className="text-lg font-bold font-mono tracking-wide text-zinc-100 flex items-center gap-2">
              <span>MATCH MODE</span>
              <span className="text-[10px] bg-emerald-500/20 text-emerald-300 border border-emerald-500/30 px-2 py-0.5 rounded-full font-mono uppercase tracking-wider">
                1v1 Heads-Up Duel
              </span>
            </h2>
            <p className="text-xs text-zinc-400">
              Face off against <span className="text-emerald-300 font-semibold">{botName}</span> until one player is eliminated!
            </p>
          </div>
        </div>

        {/* Modal Content */}
        <div className="p-6 flex flex-col gap-5 text-sm">
          {/* Starting Stacks Box */}
          <div className="bg-zinc-900/90 border border-zinc-800 rounded-xl p-3.5 flex items-center justify-between">
            <div className="flex items-center gap-3">
              <div className="w-9 h-9 rounded-lg bg-amber-500/10 border border-amber-500/30 flex items-center justify-center text-amber-400 font-mono font-bold text-xs">
                10k
              </div>
              <div>
                <div className="font-semibold text-zinc-200 text-xs">Starting Stacks</div>
                <div className="text-[11px] text-zinc-400">Both players start with equal stacks</div>
              </div>
            </div>
            <div className="font-mono font-bold text-emerald-400 text-base">
              10,000 chips each
            </div>
          </div>

          {/* Blind Structure Schedule */}
          <div className="flex flex-col gap-2">
            <div className="flex items-center justify-between text-xs font-mono font-bold text-zinc-300 px-1">
              <span className="flex items-center gap-1.5 text-zinc-200">
                <Zap className="w-3.5 h-3.5 text-amber-400" />
                Escalating Blind Schedule
              </span>
              <span className="text-zinc-500 text-[10px]">Increases each round</span>
            </div>

            <div className="grid grid-cols-2 sm:grid-cols-4 gap-2 text-center font-mono">
              <div className="bg-zinc-900 border border-zinc-800 rounded-lg p-2.5">
                <div className="text-[10px] text-zinc-400 font-semibold uppercase">Hands 1–5</div>
                <div className="text-xs font-bold text-emerald-400 mt-0.5">250 / 500</div>
                <div className="text-[9px] text-zinc-500">SB / BB</div>
              </div>

              <div className="bg-zinc-900 border border-zinc-800 rounded-lg p-2.5">
                <div className="text-[10px] text-zinc-400 font-semibold uppercase">Hands 6–9</div>
                <div className="text-xs font-bold text-amber-400 mt-0.5">1k / 2k</div>
                <div className="text-[9px] text-zinc-500">SB / BB</div>
              </div>

              <div className="bg-zinc-900 border border-zinc-800 rounded-lg p-2.5">
                <div className="text-[10px] text-zinc-400 font-semibold uppercase">Hands 10–15</div>
                <div className="text-xs font-bold text-orange-400 mt-0.5">2.5k / 5k</div>
                <div className="text-[9px] text-zinc-500">SB / BB</div>
              </div>

              <div className="bg-zinc-900 border border-zinc-800 rounded-lg p-2.5">
                <div className="text-[10px] text-zinc-400 font-semibold uppercase">Hands 16+</div>
                <div className="text-xs font-bold text-red-400 mt-0.5">5k / 10k</div>
                <div className="text-[9px] text-zinc-500">SB / BB</div>
              </div>
            </div>
          </div>

          {/* All-in & Elimination Notice */}
          <div className="bg-amber-950/40 border border-amber-700/60 rounded-xl p-3 flex items-start gap-2.5 text-amber-200/90 text-xs">
            <ShieldAlert className="w-4 h-4 text-amber-400 shrink-0 mt-0.5" />
            <div className="flex flex-col gap-0.5 leading-relaxed">
              <span className="font-semibold text-amber-300">All-In & Elimination Rules:</span>
              <span>
                If a player does not have enough chips to cover the blind or bet, they must go <strong>All-In</strong>.
                Once a player reaches <strong>0 chips</strong>, they lose the match!
              </span>
            </div>
          </div>
        </div>

        {/* Action Buttons */}
        <div className="bg-zinc-900/80 border-t border-zinc-800 px-6 py-4 flex flex-col sm:flex-row items-center justify-end gap-3">
          <button
            onClick={onCancelToInfinite}
            className="w-full sm:w-auto px-4 py-2.5 rounded-lg border border-zinc-700 bg-zinc-850 hover:bg-zinc-800 text-zinc-300 text-xs font-mono font-medium transition-colors cursor-pointer"
          >
            Play Infinite Mode
          </button>
          <button
            onClick={onConfirmStart}
            className="w-full sm:w-auto px-6 py-2.5 rounded-lg bg-emerald-600 hover:bg-emerald-500 active:bg-emerald-700 text-zinc-100 font-mono font-bold text-xs uppercase tracking-wider flex items-center justify-center gap-2 transition-colors shadow-lg shadow-emerald-950/60 cursor-pointer"
          >
            <span>I&apos;m Ready — Start Match</span>
            <ArrowRight className="w-3.5 h-3.5" />
          </button>
        </div>
      </div>
    </div>
  );
};
