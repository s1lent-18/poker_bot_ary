export interface CardData {
  code: string;
  displayCode?: string;
  rank: string;
  rankValue: number;
  suit: "clubs" | "diamonds" | "hearts" | "spades";
  suitSymbol: string;
  suitColor: "green" | "blue" | "red" | "black";
}

export interface LegalAction {
  type: "fold" | "check" | "call" | "raise";
  amount: number;
  isAllIn?: boolean;
  label?: string;
}

export interface PlayerState {
  stack: number;
  bet: number;
  isButton: boolean;
  hole: CardData[] | null;
  handRank?: string;
  name?: string;
}

export interface GameEvent {
  type: "deal" | "action" | "street_deal" | "hand_complete";
  actor?: "hero" | "bot";
  action?: string;
  amount?: number;
  pot?: number;
  count?: number;
  board?: CardData[];
  heroPayoff?: number;
  botPayoff?: number;
  winner?: "hero" | "bot" | "split";
  handNumber?: number;
  button?: number;
  sb?: number;
  bb?: number;
}

export interface SessionStats {
  handsPlayed: number;
  totalChipsWon: number;
  bbPerHand: number;
  bbPer100: number;
  holdenDecisions: number;
  holdenFallbacks: number;
  holdenFallbackPct: number;
}

export interface GameState {
  handNumber: number;
  button: number;
  street: "Preflop" | "Flop" | "Turn" | "River" | "Showdown" | "Unknown";
  pot: number;
  currentBet: number;
  toCall: number;
  toAct: "hero" | "bot" | "none";
  isTerminal: boolean;
  hero: PlayerState;
  bot: PlayerState;
  board: CardData[];
  legalActions: LegalAction[];
  events: GameEvent[];
  session: SessionStats;
  payoffs?: [number, number];
  winner?: "hero" | "bot" | "split";
}

export interface CfrVersion {
  id: string;
  filename: string;
  name: string;
  iterations: number;
  infoSets: number;
  raiseMenu: string;
  stack: string;
}

export interface BotOption {
  id: string;
  name: string;
  type: "rule" | "equity" | "cfr";
  description: string;
  versions?: CfrVersion[];
}
