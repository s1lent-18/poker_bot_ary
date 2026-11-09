import React from "react";
import { CardData } from "@/lib/types";
import { CardComponent } from "./CardComponent";
import { Eye, EyeOff, X } from "lucide-react";

interface BotCheckPanelProps {
  botCheckEnabled: boolean;
  onToggleBotCheck: () => void;
  botActualHole: CardData[] | null;
  botActualRank?: string;
  foldedPlayer?: "hero" | "bot" | "none";
  street?: string;
  isTerminal: boolean;
  isOpen: boolean;
  onClose: () => void;
  botName: string;
  fourColor: boolean;
}

export const BotCheckPanel: React.FC<BotCheckPanelProps> = ({
  botCheckEnabled,
  onToggleBotCheck,
  botActualHole,
  botActualRank,
  foldedPlayer,
  street,
  isTerminal,
  isOpen,
  onClose,
  botName,
  fourColor,
}) => {
  // If botCheck mode is OFF or user closed the side panel
  if (!botCheckEnabled) return null;
  if (!isOpen) return null;

  const hasFoldData = isTerminal && (foldedPlayer === "bot" || foldedPlayer === "hero") && botActualHole;

  return (
    <div className="fixed right-3 sm:right-6 top-16 z-40 w-72 sm:w-80 bg-zinc-950 border border-zinc-750 rounded-xl shadow-xl overflow-hidden select-none animate-in fade-in slide-in-from-right-4 duration-200">
      {/* Header */}
      <div className="bg-zinc-900 border-b border-zinc-800 px-3.5 py-2.5 flex items-center justify-between">
        <div className="flex items-center gap-2">
          <Eye className="w-4 h-4 text-emerald-400" />
          <span className="font-mono text-xs font-bold text-zinc-100 uppercase tracking-wider">
            BotCheck Mode
          </span>
          <span className="text-[10px] bg-emerald-950 text-emerald-300 border border-emerald-800/80 px-1.5 py-0.2 rounded font-mono font-semibold">
            ON
          </span>
        </div>
        <button
          onClick={onClose}
          className="p-1 rounded text-zinc-400 hover:text-zinc-100 hover:bg-zinc-800 transition-colors cursor-pointer"
          title="Minimize panel"
        >
          <X className="w-3.5 h-3.5" />
        </button>
      </div>

      {/* Content */}
      <div className="p-3.5 flex flex-col gap-3 font-mono text-xs">
        {hasFoldData ? (
          <>
            {/* Fold event description */}
            <div className="flex items-center justify-between text-xs pb-2 border-b border-zinc-850">
              <span className="text-zinc-400">Fold Event:</span>
              <span
                className={`font-bold ${
                  foldedPlayer === "bot" ? "text-amber-400" : "text-red-400"
                }`}
              >
                {foldedPlayer === "bot" ? `${botName} Folded` : "You Folded"}
              </span>
            </div>

            {/* Revealed Bot Cards */}
            <div className="flex flex-col items-center gap-2 bg-zinc-900/80 border border-zinc-800 rounded-lg p-3">
              <span className="text-[11px] text-zinc-400 font-medium">
                {botName}&apos;s Mucked Hand:
              </span>
              <div className="flex gap-2.5 my-1">
                {botActualHole && botActualHole.length === 2 && (
                  <>
                    <CardComponent card={botActualHole[0]} fourColor={fourColor} />
                    <CardComponent card={botActualHole[1]} fourColor={fourColor} />
                  </>
                )}
              </div>
              {botActualRank && (
                <div className="text-xs font-bold text-amber-300 uppercase tracking-wide">
                  {botActualRank}
                </div>
              )}
            </div>

            {/* Context Note */}
            <div className="text-[11px] text-zinc-500 text-center leading-relaxed">
              {foldedPlayer === "bot"
                ? `Bot folded on the ${street}. This shows what the bot was holding when it laid down its hand.`
                : `You folded on the ${street}. This reveals what the bot was holding against you.`}
            </div>
          </>
        ) : (
          <div className="py-4 text-center flex flex-col items-center gap-2 text-zinc-400">
            <Eye className="w-5 h-5 text-zinc-500" />
            <span className="text-xs text-zinc-300 font-semibold">Ready for Fold Detection</span>
            <span className="text-[11px] text-zinc-500 leading-normal">
              When you or {botName} folds, the bot&apos;s cards and hand rank will automatically appear here.
            </span>
          </div>
        )}

        {/* Footer Toggle */}
        <div className="pt-2 border-t border-zinc-850 flex items-center justify-between text-[11px]">
          <span className="text-zinc-500">BotCheck is enabled</span>
          <button
            onClick={onToggleBotCheck}
            className="text-red-400 hover:text-red-300 underline cursor-pointer"
          >
            Turn Off
          </button>
        </div>
      </div>
    </div>
  );
};
