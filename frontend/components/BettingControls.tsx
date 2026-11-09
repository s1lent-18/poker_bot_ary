import React, { useEffect } from "react";
import { LegalAction } from "@/lib/types";

interface BettingControlsProps {
  legalActions: LegalAction[];
  toAct: "hero" | "bot" | "none";
  isTerminal: boolean;
  toCall: number;
  pot: number;
  heroStack: number;
  heroBet: number;
  onAction: (action: "fold" | "check" | "call" | "raise", amount?: number) => void;
  onNextHand: () => void;
  isPending: boolean;
}

export const BettingControls: React.FC<BettingControlsProps> = ({
  legalActions,
  toAct,
  isTerminal,
  toCall,
  pot,
  heroBet,
  onAction,
  onNextHand,
  isPending,
}) => {
  const canFold = legalActions.some((a) => a.type === "fold");
  const canCheck = legalActions.some((a) => a.type === "check");
  const callAction = legalActions.find((a) => a.type === "call");
  const raiseActions = legalActions.filter((a) => a.type === "raise");

  // Keyboard shortcut listener
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      // Ignore if user is inside an input
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement) {
        return;
      }

      if (isTerminal) {
        if (e.key === "Enter" || e.key === " " || e.key === "n" || e.key === "N") {
          e.preventDefault();
          if (!isPending) onNextHand();
        }
        return;
      }

      if (toAct !== "hero" || isPending) return;

      if ((e.key === "f" || e.key === "F") && canFold) {
        e.preventDefault();
        onAction("fold");
      } else if (e.key === "c" || e.key === "C" || e.key === " ") {
        e.preventDefault();
        if (canCheck) onAction("check");
        else if (callAction) onAction("call");
      } else if ((e.key === "a" || e.key === "A") && raiseActions.length > 0) {
        e.preventDefault();
        const allInAction = raiseActions[raiseActions.length - 1];
        onAction("raise", allInAction.amount);
      } else if (e.key >= "1" && e.key <= "9") {
        const idx = Number(e.key) - 1;
        if (idx < raiseActions.length) {
          e.preventDefault();
          onAction("raise", raiseActions[idx].amount);
        }
      }
    };

    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [isTerminal, toAct, isPending, canFold, canCheck, callAction, raiseActions, onAction, onNextHand]);

  // Terminal state: Big clean "Next Hand" button
  if (isTerminal) {
    return (
      <div className="w-full max-w-xl mx-auto py-3 px-4 flex items-center justify-center select-none">
        <button
          onClick={onNextHand}
          disabled={isPending}
          className="w-full sm:w-auto min-w-[240px] px-8 py-3.5 bg-emerald-700 hover:bg-emerald-600 active:bg-emerald-800 text-zinc-100 font-bold font-mono rounded-lg border border-emerald-500 text-sm uppercase tracking-wider transition-colors shadow-sm cursor-pointer flex items-center justify-center gap-2"
        >
          <span>Next Hand</span>
          <span className="text-xs bg-emerald-800 text-emerald-200 px-2 py-0.5 rounded font-mono">
            Space ↵
          </span>
        </button>
      </div>
    );
  }

  const isHeroTurn = toAct === "hero";

  // When not Hero's turn
  if (!isHeroTurn) {
    return (
      <div className="w-full max-w-xl mx-auto py-4 px-4 flex items-center justify-center text-zinc-400 font-mono text-xs select-none">
        <div className="flex items-center gap-2 bg-zinc-900 border border-zinc-800 rounded-full px-4 py-1.5 animate-pulse">
          <span className="w-2 h-2 rounded-full bg-amber-400" />
          <span>Opponent is thinking...</span>
        </div>
      </div>
    );
  }

  // Pre-calculate raise descriptions
  const potAfterCall = pot + toCall;

  return (
    <div className="w-full max-w-3xl mx-auto py-3 px-3 sm:px-4 flex flex-col gap-2 select-none">
      {/* Primary Actions Grid */}
      <div className="flex flex-wrap items-center justify-center gap-2 sm:gap-3">
        {/* Fold Button */}
        {canFold && (
          <button
            onClick={() => onAction("fold")}
            disabled={isPending}
            className="flex-1 min-w-[100px] sm:min-w-[120px] py-3 px-4 rounded-lg border border-red-900/80 bg-zinc-900 hover:bg-red-950/60 active:bg-red-900/60 text-red-300 font-mono font-bold text-xs sm:text-sm tracking-wide uppercase transition-colors cursor-pointer flex flex-col items-center justify-center gap-0.5"
          >
            <span>Fold</span>
            <span className="text-[10px] text-red-500 font-normal">Hotkey: F</span>
          </button>
        )}

        {/* Check Button */}
        {canCheck && (
          <button
            onClick={() => onAction("check")}
            disabled={isPending}
            className="flex-1 min-w-[110px] sm:min-w-[130px] py-3 px-4 rounded-lg border border-zinc-700 bg-zinc-850 hover:bg-zinc-750 active:bg-zinc-700 text-zinc-100 font-mono font-bold text-xs sm:text-sm tracking-wide uppercase transition-colors cursor-pointer flex flex-col items-center justify-center gap-0.5"
          >
            <span>Check</span>
            <span className="text-[10px] text-zinc-400 font-normal">Space / C</span>
          </button>
        )}

        {/* Call Button */}
        {callAction && (
          <button
            onClick={() => onAction("call")}
            disabled={isPending}
            className="flex-1 min-w-[120px] sm:min-w-[140px] py-3 px-4 rounded-lg border border-emerald-600 bg-emerald-950/80 hover:bg-emerald-900 active:bg-emerald-800 text-emerald-200 font-mono font-bold text-xs sm:text-sm tracking-wide uppercase transition-colors cursor-pointer flex flex-col items-center justify-center gap-0.5"
          >
            <span>Call {toCall > 0 ? toCall.toLocaleString() : ""}</span>
            <span className="text-[10px] text-emerald-400 font-normal">Space / C</span>
          </button>
        )}

        {/* Engine-Compatible Sized Raise & All-In Buttons */}
        {raiseActions.map((raiseAct, idx) => {
          const isAllIn = raiseAct.isAllIn || idx === raiseActions.length - 1;
          const additionalChips = raiseAct.amount - heroBet;

          // Estimate pot fraction label
          let sizeLabel = "";
          if (isAllIn) {
            sizeLabel = "All-In";
          } else {
            const fraction = additionalChips / Math.max(1, potAfterCall);
            if (Math.abs(fraction - 1.0) < 0.25) sizeLabel = "1x Pot";
            else if (Math.abs(fraction - 0.5) < 0.2) sizeLabel = "½ Pot";
            else sizeLabel = `+${additionalChips}`;
          }

          if (isAllIn) {
            return (
              <button
                key={idx}
                onClick={() => onAction("raise", raiseAct.amount)}
                disabled={isPending}
                className="flex-1 min-w-[120px] sm:min-w-[140px] py-3 px-4 rounded-lg border border-amber-600/90 bg-amber-950/60 hover:bg-amber-900/80 active:bg-amber-800 text-amber-200 font-mono font-bold text-xs sm:text-sm tracking-wide uppercase transition-colors cursor-pointer flex flex-col items-center justify-center gap-0.5"
              >
                <span>ALL-IN ({raiseAct.amount.toLocaleString()})</span>
                <span className="text-[10px] text-amber-400 font-normal">Hotkey: A</span>
              </button>
            );
          }

          return (
            <button
              key={idx}
              onClick={() => onAction("raise", raiseAct.amount)}
              disabled={isPending}
              className="flex-1 min-w-[110px] sm:min-w-[130px] py-3 px-4 rounded-lg border border-zinc-700 bg-zinc-900 hover:bg-zinc-800 active:bg-zinc-700 text-zinc-100 font-mono font-bold text-xs sm:text-sm tracking-wide uppercase transition-colors cursor-pointer flex flex-col items-center justify-center gap-0.5"
            >
              <span>Raise to {raiseAct.amount.toLocaleString()}</span>
              <span className="text-[10px] text-zinc-400 font-normal">
                {sizeLabel} {idx < 9 ? `[${idx + 1}]` : ""}
              </span>
            </button>
          );
        })}
      </div>
    </div>
  );
};
