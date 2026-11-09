import React from "react";
import { CardData } from "@/lib/types";

interface CardProps {
  card?: CardData | null;
  faceDown?: boolean;
  fourColor?: boolean;
  className?: string;
  dimmed?: boolean;
}

export const CardComponent: React.FC<CardProps> = ({
  card,
  faceDown = false,
  fourColor = true,
  className = "",
  dimmed = false,
}) => {
  if (faceDown || !card) {
    return (
      <div
        className={`w-14 h-20 sm:w-16 sm:h-24 md:w-20 md:h-28 rounded-md border border-zinc-700 bg-zinc-800 flex items-center justify-center select-none shadow-sm transition-all overflow-hidden shrink-0 ${
          dimmed ? "opacity-40" : ""
        } ${className}`}
      >
        <div className="w-10 h-16 sm:w-12 sm:h-20 md:w-16 md:h-24 border border-zinc-700/80 rounded flex items-center justify-center bg-zinc-850">
          <div className="w-3.5 h-3.5 border border-zinc-600 rotate-45 flex items-center justify-center">
            <div className="w-1.5 h-1.5 bg-zinc-600 rounded-full" />
          </div>
        </div>
      </div>
    );
  }

  // Format rank: Always show "10" cleanly instead of "T"
  const rankDisplay = card.rank === "T" || card.rank === "10" ? "10" : card.rank;

  // Determine suit color based on fourColor setting
  let textColor = "text-zinc-900";
  if (fourColor) {
    switch (card.suit) {
      case "hearts":
        textColor = "text-red-600";
        break;
      case "diamonds":
        textColor = "text-blue-600";
        break;
      case "clubs":
        textColor = "text-emerald-700";
        break;
      case "spades":
        textColor = "text-zinc-900";
        break;
    }
  } else {
    textColor = card.suit === "hearts" || card.suit === "diamonds" ? "text-red-600" : "text-zinc-900";
  }

  return (
    <div
      className={`relative w-14 h-20 sm:w-16 sm:h-24 md:w-20 md:h-28 rounded-md border border-zinc-300 bg-white flex flex-col justify-between p-1 sm:p-1.5 select-none transition-all shadow-sm overflow-hidden shrink-0 ${
        dimmed ? "opacity-40" : ""
      } ${className}`}
    >
      {/* Top Left Index */}
      <div className={`flex flex-col items-center w-4 sm:w-5 leading-none z-10 ${textColor}`}>
        <span
          className={`font-bold font-mono tracking-tighter ${
            rankDisplay === "10"
              ? "text-[10px] sm:text-xs md:text-sm"
              : "text-xs sm:text-sm md:text-base"
          }`}
        >
          {rankDisplay}
        </span>
        <span className="text-[9px] sm:text-[11px] md:text-xs font-semibold leading-none mt-0.5">
          {card.suitSymbol}
        </span>
      </div>

      {/* Center Large Suit Symbol (Absolutely centered so it never causes card overflow) */}
      <div
        className={`absolute inset-0 flex items-center justify-center pointer-events-none z-0 ${textColor}`}
      >
        <span className="text-xl sm:text-2xl md:text-3xl font-normal leading-none opacity-85">
          {card.suitSymbol}
        </span>
      </div>

      {/* Bottom Right Inverted Index */}
      <div
        className={`flex flex-col items-center w-4 sm:w-5 leading-none rotate-180 self-end z-10 ${textColor}`}
      >
        <span
          className={`font-bold font-mono tracking-tighter ${
            rankDisplay === "10"
              ? "text-[10px] sm:text-xs md:text-sm"
              : "text-xs sm:text-sm md:text-base"
          }`}
        >
          {rankDisplay}
        </span>
        <span className="text-[9px] sm:text-[11px] md:text-xs font-semibold leading-none mt-0.5">
          {card.suitSymbol}
        </span>
      </div>
    </div>
  );
};
