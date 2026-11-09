import React from "react";
import { GameState } from "@/lib/types";
import { CardComponent } from "./CardComponent";

interface PokerTableProps {
  gameState: GameState | null;
  fourColor: boolean;
  botLastActionText: string | null;
  isBotThinking: boolean;
}

export const PokerTable: React.FC<PokerTableProps> = ({
  gameState,
  fourColor,
  botLastActionText,
  isBotThinking,
}) => {
  if (!gameState) {
    return (
      <div className="flex-1 flex items-center justify-center p-6 text-zinc-500 font-mono text-sm">
        Initializing poker table...
      </div>
    );
  }

  const { hero, bot, board, pot, street, button, isTerminal, payoffs, winner } = gameState;

  // Outcome banner
  let resultBanner = null;
  if (isTerminal && payoffs) {
    if (winner === "hero") {
      resultBanner = (
        <div className="bg-emerald-950 border border-emerald-500 text-emerald-200 font-mono text-xs sm:text-sm px-5 py-2 rounded-md uppercase tracking-wider font-bold shadow-sm">
          You win +{payoffs[0].toLocaleString()} chips {hero.handRank ? `(${hero.handRank})` : ""}
        </div>
      );
    } else if (winner === "bot") {
      resultBanner = (
        <div className="bg-red-950 border border-red-500 text-red-200 font-mono text-xs sm:text-sm px-5 py-2 rounded-md uppercase tracking-wider font-bold shadow-sm">
          {bot.name} wins +{payoffs[1].toLocaleString()} chips {bot.handRank ? `(${bot.handRank})` : ""}
        </div>
      );
    } else {
      resultBanner = (
        <div className="bg-zinc-800 border border-zinc-500 text-zinc-100 font-mono text-xs sm:text-sm px-5 py-2 rounded-md uppercase tracking-wider font-bold shadow-sm">
          Split Pot
        </div>
      );
    }
  }

  return (
    <div className="w-full max-w-4xl mx-auto px-2 sm:px-4 flex flex-col items-center justify-center select-none">
      {/* Felt Canvas */}
      <div className="relative w-full min-h-[490px] sm:min-h-[530px] bg-[#122e1e] border-2 border-[#1c472f] rounded-[40px] sm:rounded-[60px] flex flex-col justify-between p-4 sm:p-6 shadow-sm">
        {/* Subtle inner boundary line */}
        <div className="absolute inset-2 sm:inset-3 border border-[#183c27] rounded-[34px] sm:rounded-[52px] pointer-events-none" />

        {/* ----------------- 1. TOP: BOT OPPONENT ----------------- */}
        <div className="relative z-10 flex flex-col items-center gap-1.5">
          {/* Action Speech Bubble or Thinking */}
          <div className="h-6 flex items-center justify-center">
            {isBotThinking ? (
              <span className="text-[11px] font-mono text-amber-300 bg-zinc-900 border border-zinc-700 px-3 py-0.5 rounded-full tracking-wide">
                Opponent is thinking...
              </span>
            ) : botLastActionText ? (
              <span className="text-[11px] font-mono font-bold text-zinc-100 bg-zinc-900 border border-zinc-700 px-3 py-0.5 rounded-full uppercase tracking-wider">
                {botLastActionText}
              </span>
            ) : null}
          </div>

          {/* Bot Player Box & Hole Cards */}
          <div className="flex items-center gap-3 sm:gap-5">
            {/* Bot Hole Cards */}
            <div className="flex gap-2">
              {bot.hole && bot.hole.length === 2 ? (
                <>
                  <CardComponent card={bot.hole[0]} fourColor={fourColor} />
                  <CardComponent card={bot.hole[1]} fourColor={fourColor} />
                </>
              ) : (
                <>
                  <CardComponent faceDown />
                  <CardComponent faceDown />
                </>
              )}
            </div>

            {/* Bot Info Card */}
            <div className="flex flex-col gap-1 bg-zinc-900 border border-zinc-750 rounded-lg px-3.5 py-2 min-w-[130px] sm:min-w-[160px] shadow-sm">
              <div className="flex items-center justify-between gap-2">
                <span className="text-xs font-bold text-zinc-200 truncate">{bot.name}</span>
                {button === 1 && (
                  <span
                    className="w-4 h-4 rounded-full bg-amber-400 text-zinc-950 font-bold text-[10px] flex items-center justify-center shrink-0"
                    title="Dealer Button (Small Blind)"
                  >
                    D
                  </span>
                )}
              </div>
              <div className="flex items-baseline gap-1.5 font-mono text-xs">
                <span className="text-zinc-400">Stack:</span>
                <span className="text-emerald-400 font-bold text-sm">
                  {bot.stack.toLocaleString()}
                </span>
              </div>
              {bot.handRank && isTerminal && (
                <div className="text-[10px] font-mono text-amber-300 truncate font-semibold uppercase">
                  {bot.handRank}
                </div>
              )}
            </div>
          </div>

          {/* Bot Bet Chips on the Felt */}
          <div className="h-6 flex items-center justify-center">
            {bot.bet > 0 && (
              <div className="flex items-center gap-1.5 bg-zinc-900 border border-amber-500/80 rounded-full px-3 py-0.5 font-mono text-xs text-amber-300 font-bold shadow-sm">
                <span className="w-2 h-2 rounded-full bg-amber-400" />
                <span>Bet: {bot.bet.toLocaleString()}</span>
              </div>
            )}
          </div>
        </div>

        {/* ----------------- 2. CENTER: POT & BOARD ----------------- */}
        <div className="relative z-10 flex flex-col items-center justify-center my-auto gap-2.5">
          {/* Pot & Street Pill */}
          <div className="flex items-center gap-2.5 bg-zinc-950 border border-zinc-700 rounded-full px-5 py-1.5 font-mono shadow-sm">
            <span className="text-zinc-400 text-xs uppercase tracking-wider font-medium">Pot</span>
            <span className="text-zinc-100 text-base sm:text-lg font-bold">
              {pot.toLocaleString()}
            </span>
            <span className="text-zinc-600 text-xs">|</span>
            <span className="text-emerald-400 text-xs uppercase font-bold tracking-wider">{street}</span>
          </div>

          {/* 5 Community Cards with generous spacing */}
          <div className="flex gap-2 sm:gap-3 items-center justify-center">
            {[0, 1, 2, 3, 4].map((idx) => {
              const card = board[idx];
              if (card) {
                return (
                  <CardComponent key={idx} card={card} fourColor={fourColor} />
                );
              }
              // Empty slot placeholder
              return (
                <div
                  key={idx}
                  className="w-14 h-20 sm:w-16 sm:h-24 md:w-20 md:h-28 rounded-md border border-dashed border-[#1f4a30] bg-[#0f2418]/60 flex items-center justify-center"
                >
                  <span className="text-[10px] font-mono font-semibold text-[#296340] select-none">
                    {idx < 3 ? "FLOP" : idx === 3 ? "TURN" : "RIVER"}
                  </span>
                </div>
              );
            })}
          </div>

          {/* Showdown Result Banner */}
          {resultBanner && <div className="mt-1">{resultBanner}</div>}
        </div>

        {/* ----------------- 3. BOTTOM: HERO (YOU) ----------------- */}
        <div className="relative z-10 flex flex-col items-center gap-1.5">
          {/* Hero Bet Chips on the Felt */}
          <div className="h-6 flex items-center justify-center">
            {hero.bet > 0 && (
              <div className="flex items-center gap-1.5 bg-zinc-900 border border-amber-500/80 rounded-full px-3 py-0.5 font-mono text-xs text-amber-300 font-bold shadow-sm">
                <span className="w-2 h-2 rounded-full bg-amber-400" />
                <span>Bet: {hero.bet.toLocaleString()}</span>
              </div>
            )}
          </div>

          {/* Hero Player Box & Hole Cards */}
          <div className="flex items-center gap-3 sm:gap-5">
            {/* Hero Info Card */}
            <div className="flex flex-col gap-1 bg-zinc-900 border border-zinc-750 rounded-lg px-3.5 py-2 min-w-[130px] sm:min-w-[160px] shadow-sm">
              <div className="flex items-center justify-between gap-2">
                {button === 0 && (
                  <span
                    className="w-4 h-4 rounded-full bg-amber-400 text-zinc-950 font-bold text-[10px] flex items-center justify-center shrink-0"
                    title="Dealer Button (Small Blind)"
                  >
                    D
                  </span>
                )}
                <span className="text-xs font-bold text-zinc-100 ml-auto">You (Hero)</span>
              </div>
              <div className="flex items-baseline justify-end gap-1.5 font-mono text-xs">
                <span className="text-zinc-400">Stack:</span>
                <span className="text-emerald-400 font-bold text-sm">
                  {hero.stack.toLocaleString()}
                </span>
              </div>
              {hero.handRank && (
                <div className="text-[10px] font-mono text-emerald-300 truncate font-semibold uppercase tracking-wider text-right">
                  {hero.handRank}
                </div>
              )}
            </div>

            {/* Hero Hole Cards */}
            <div className="flex gap-2">
              {hero.hole && hero.hole.length === 2 ? (
                <>
                  <CardComponent card={hero.hole[0]} fourColor={fourColor} />
                  <CardComponent card={hero.hole[1]} fourColor={fourColor} />
                </>
              ) : (
                <>
                  <CardComponent faceDown />
                  <CardComponent faceDown />
                </>
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};
