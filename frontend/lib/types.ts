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
  actualHole?: CardData[] | null;
  actualHandRank?: string;
  folded?: boolean;
  foldedPlayer?: "hero" | "bot" | "none";
  handRank?: string;
  name?: string;
}

export interface GameEvent {
  type: "deal" | "action" | "street_deal" | "hand_complete" | "rebuy";
  actor?: "hero" | "bot";
  action?: string;
  amount?: number;
  pot?: number;
  count?: number;
  board?: CardData[];
  heroPayoff?: number;
  botPayoff?: number;
  winner?: "hero" | "bot" | "split";
  folded?: boolean;
  foldedPlayer?: "hero" | "bot" | "none";
  player?: "hero" | "bot";
  handNumber?: number;
  button?: number;
  sb?: number;
  bb?: number;
  isMatchOver?: boolean;
  matchWinner?: "hero" | "bot" | string;
}

export interface SessionStats {
  handsPlayed: number;
  totalChipsWon: number;
  bbPerHand: number;
  bbPer100: number;
  holdenDecisions: number;
  holdenFallbacks: number;
  holdenFallbackPct: number;
  autoRebuy?: boolean;
  heroBankroll?: number;
  botBankroll?: number;
}

export interface MatchInfo {
  isMatchMode: boolean;
  matchHandNumber: number;
  sb: number;
  bb: number;
  nextIncreaseHand: number;
  nextSb: number;
  nextBb: number;
  isMatchOver: boolean;
  matchWinner?: "hero" | "bot" | string;
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
  gameMode?: "infinite" | "match";
  match?: MatchInfo;
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
