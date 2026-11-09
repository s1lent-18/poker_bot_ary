import React, { useEffect } from "react";
import { GameEvent } from "@/lib/types";
import { X, History } from "lucide-react";

interface ActionHistoryLogProps {
  events: GameEvent[];
  isOpen: boolean;
  onClose: () => void;
}

export const ActionHistoryLog: React.FC<ActionHistoryLogProps> = ({
  events,
  isOpen,
  onClose,
}) => {
  // Close on Escape key
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.key === "Escape" && isOpen) {
        onClose();
      }
    };
    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, [isOpen, onClose]);

  if (!isOpen) return null;

  return (
    <div className="fixed inset-0 z-50 flex justify-end bg-black/60 backdrop-blur-xs select-none">
      {/* Click outside to close */}
      <div className="flex-1" onClick={onClose} />

      {/* Drawer */}
      <div className="w-full max-w-md bg-zinc-950 border-l border-zinc-800 h-full flex flex-col shadow-2xl">
        {/* Header */}
        <div className="p-3.5 border-b border-zinc-800 flex items-center justify-between bg-zinc-900/90">
          <div className="flex items-center gap-2 text-zinc-100 font-mono text-xs font-bold uppercase tracking-wider">
            <History className="w-4 h-4 text-emerald-400" />
            <span>Hand History Log</span>
            <span className="text-zinc-500 font-normal">({events.length} events)</span>
          </div>
          <button
            onClick={onClose}
            className="p-1 rounded text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800 transition-colors cursor-pointer"
            title="Close (Esc)"
          >
            <X className="w-4 h-4" />
          </button>
        </div>

        {/* Scrollable event list */}
        <div className="flex-1 overflow-y-auto p-4 flex flex-col gap-1.5 font-mono text-xs text-zinc-300">
          {events.length === 0 ? (
            <div className="py-8 text-center text-zinc-600 italic">No events recorded yet.</div>
          ) : (
            events.map((evt, idx) => {
              if (evt.type === "deal") {
                return (
                  <div
                    key={idx}
                    className="mt-2 pt-2 border-t border-zinc-800/80 text-zinc-400 font-bold flex items-center justify-between text-[11px]"
                  >
                    <span>--- Hand #{evt.handNumber} ---</span>
                    <span className="text-zinc-500 font-normal">
                      Button: {evt.button === 0 ? "You" : "Bot"} (Blinds {evt.sb}/{evt.bb})
                    </span>
                  </div>
                );
              }

              if (evt.type === "action") {
                const isHero = evt.actor === "hero";
                return (
                  <div key={idx} className="flex items-center justify-between py-0.5 text-xs">
                    <div className="flex items-center gap-2">
                      <span
                        className={`font-bold ${
                          isHero ? "text-emerald-400" : "text-amber-400"
                        }`}
                      >
                        {isHero ? "You:" : "Bot:"}
                      </span>
                      <span>{evt.action}</span>
                    </div>
                    {evt.pot !== undefined && (
                      <span className="text-zinc-500 text-[10px]">Pot: {evt.pot}</span>
                    )}
                  </div>
                );
              }

              if (evt.type === "street_deal") {
                const streetName = evt.count === 3 ? "Flop" : evt.count === 4 ? "Turn" : "River";
                const cardsStr = evt.board
                  ? evt.board.map((c) => (c.displayCode ? c.displayCode : c.code)).join(" ")
                  : "";
                return (
                  <div
                    key={idx}
                    className="my-1 px-2.5 py-1 rounded bg-zinc-900 border border-zinc-800 text-zinc-200 font-semibold text-xs flex items-center justify-between"
                  >
                    <span className="text-emerald-400 uppercase tracking-wide">
                      ** {streetName}
                    </span>
                    <span className="font-bold text-zinc-100">{cardsStr}</span>
                  </div>
                );
              }

              if (evt.type === "hand_complete") {
                return (
                  <div
                    key={idx}
                    className={`mt-1 mb-2 p-2 rounded border text-xs font-bold ${
                      evt.winner === "hero"
                        ? "bg-emerald-950/60 border-emerald-800/80 text-emerald-300"
                        : evt.winner === "bot"
                        ? "bg-red-950/60 border-red-800/80 text-red-300"
                        : "bg-zinc-900 border-zinc-700 text-zinc-300"
                    }`}
                  >
                    {evt.winner === "hero"
                      ? `=> You won +${evt.heroPayoff} chips`
                      : evt.winner === "bot"
                      ? `=> Bot won +${evt.botPayoff} chips`
                      : "=> Split pot"}
                  </div>
                );
              }

              return null;
            })
          )}
        </div>
      </div>
    </div>
  );
};
