"use client";

import React, { useState, useEffect, useCallback, useRef } from "react";
import { GameState, BotOption, GameEvent } from "@/lib/types";
import { HeaderBar } from "@/components/HeaderBar";
import { PokerTable } from "@/components/PokerTable";
import { BettingControls } from "@/components/BettingControls";
import { ActionHistoryLog } from "@/components/ActionHistoryLog";
import { BotCheckPanel } from "@/components/BotCheckPanel";
import { MatchReadyModal } from "@/components/MatchReadyModal";
import { MatchOverModal } from "@/components/MatchOverModal";
import { sound } from "@/lib/audio";

const sleep = (ms: number) => new Promise((resolve) => setTimeout(resolve, ms));

export default function PokerApp() {
  const [bots, setBots] = useState<BotOption[]>([]);
  const [selectedBotId, setSelectedBotId] = useState<string>("cfr");
  const [selectedVersion, setSelectedVersion] = useState<string>("versions/run4.cfr");

  // Game mode: "infinite" (default continuous mode) vs "match" (10k elimination duel)
  const [gameMode, setGameMode] = useState<"infinite" | "match">("infinite");
  const [isMatchReadyOpen, setIsMatchReadyOpen] = useState<boolean>(false);
  const [isMatchOverOpen, setIsMatchOverOpen] = useState<boolean>(false);

  const [gameState, setGameState] = useState<GameState | null>(null);
  const [isPending, setIsPending] = useState<boolean>(false);
  const [botLastActionText, setBotLastActionText] = useState<string | null>(null);
  const [isBotThinking, setIsBotThinking] = useState<boolean>(false);

  const [fourColor, setFourColor] = useState<boolean>(true);
  const [isMuted, setIsMuted] = useState<boolean>(false);
  const [historyEvents, setHistoryEvents] = useState<GameEvent[]>([]);
  const [isHistoryOpen, setIsHistoryOpen] = useState<boolean>(false);
  const [errorMsg, setErrorMsg] = useState<string | null>(null);
  const [rebuyAlert, setRebuyAlert] = useState<string | null>(null);

  // Stack mode for Infinite mode: false = persistent stacks across rounds, true = auto reset 10k each round
  const [autoRebuy, setAutoRebuy] = useState<boolean>(false);

  // BotCheck mode state
  const [botCheckEnabled, setBotCheckEnabled] = useState<boolean>(true);
  const [isBotCheckOpen, setIsBotCheckOpen] = useState<boolean>(false);

  // Keep track of how many board cards are currently displayed to animate one-by-one
  const displayedBoardCountRef = useRef<number>(0);

  // Keyboard shortcut listener for History toggle (H)
  useEffect(() => {
    const handleKeyDown = (e: KeyboardEvent) => {
      if (e.target instanceof HTMLInputElement || e.target instanceof HTMLTextAreaElement) return;
      if (e.key === "h" || e.key === "H") {
        e.preventDefault();
        setIsHistoryOpen((v) => !v);
      }
    };
    window.addEventListener("keydown", handleKeyDown);
    return () => window.removeEventListener("keydown", handleKeyDown);
  }, []);

  // Fetch available bots on mount
  useEffect(() => {
    async function loadBots() {
      try {
        const res = await fetch("/api/bots");
        if (!res.ok) throw new Error("Failed to load bots");
        const data = await res.json();
        if (data.bots && data.bots.length > 0) {
          setBots(data.bots);

          // Find default Hold'em version
          const cfrBot = data.bots.find((b: BotOption) => b.id === "cfr");
          if (cfrBot && cfrBot.versions && cfrBot.versions.length > 0) {
            const run4 = cfrBot.versions.find((v: { filename: string }) => v.filename === "run4.cfr");
            setSelectedVersion(run4 ? run4.id : cfrBot.versions[0].id);
          }
        }
      } catch (err: unknown) {
        console.error("Error loading bots:", err);
        setErrorMsg("Unable to connect to poker engine backend on port 8085.");
      }
    }
    loadBots();
  }, []);

  // Playback server events sequentially with natural poker pacing & one-by-one card reveals
  const playbackEvents = useCallback(
    async (finalState: GameState, events: GameEvent[]) => {
      for (const evt of events) {
        if (evt.type === "rebuy") {
          const playerName = evt.player === "hero" ? "You" : "Bot";
          setRebuyAlert(`${playerName} fell below minimum buy-in and rebought 10,000 chips.`);
          sound.playChip();
          await sleep(600);
        } else if (evt.type === "action" && evt.actor === "hero") {
          continue;
        } else if (evt.type === "action" && evt.actor === "bot") {
          setIsBotThinking(true);
          await sleep(550);
          setIsBotThinking(false);

          setBotLastActionText(evt.action || null);
          if (evt.action?.includes("fold")) {
            sound.playFold();
          } else if (evt.action?.includes("check")) {
            sound.playCheck();
          } else if (evt.action?.includes("all-in") || evt.action?.includes("ALL-IN")) {
            sound.playAllIn();
          } else {
            sound.playChip();
          }

          if (evt.pot !== undefined) {
            setGameState((prev) => (prev ? { ...prev, pot: evt.pot! } : prev));
          }
          await sleep(250);
        } else if (evt.type === "street_deal") {
          // Open community cards ONE BY ONE with a noticeable delay
          const targetCards = evt.board || [];
          const startIdx = displayedBoardCountRef.current;

          for (let i = startIdx; i < targetCards.length; i++) {
            await sleep(240); // small noticeable delay between each card!
            sound.playCardDeal();
            displayedBoardCountRef.current = i + 1;
            const partial = targetCards.slice(0, i + 1);
            setGameState((prev) => (prev ? { ...prev, board: partial } : prev));
          }
          await sleep(260);
        } else if (evt.type === "hand_complete") {
          await sleep(450);

          // Check if match mode elimination occurred!
          if (evt.isMatchOver || finalState.match?.isMatchOver) {
            const winner = evt.matchWinner || finalState.match?.matchWinner;
            if (winner === "hero") {
              sound.playMatchVictory();
            } else {
              sound.playMatchDefeat();
            }
            // Delay popup slightly so user can view final showdown hand
            setTimeout(() => {
              setIsMatchOverOpen(true);
            }, 1000);
          } else if (evt.winner === "hero") {
            sound.playWin();
          }

          // If someone folded and BotCheck mode is enabled, open the side window!
          if (evt.folded && botCheckEnabled && !evt.isMatchOver) {
            setIsBotCheckOpen(true);
          }
        }
      }

      displayedBoardCountRef.current = finalState.board.length;
      // Apply the final complete state
      setGameState(finalState);
    },
    [botCheckEnabled]
  );

  // Start new game
  const startNewGame = useCallback(
    async (
      botId: string,
      versionId: string,
      rebuyMode: boolean,
      mode: "infinite" | "match" = "infinite"
    ) => {
      setIsPending(true);
      setErrorMsg(null);
      setRebuyAlert(null);
      setBotLastActionText(null);
      displayedBoardCountRef.current = 0;
      setGameMode(mode);

      try {
        const res = await fetch("/api/game/new", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            bot: botId,
            version: versionId,
            autoRebuy: rebuyMode,
            gameMode: mode,
          }),
        });
        if (!res.ok) throw new Error("Failed to start game");
        const data: GameState = await res.json();

        // Initial deal sound
        sound.playCardDeal();

        // Process any initial bot preflop action with pacing
        const botActs = data.events.filter((e) => e.actor === "bot");
        if (botActs.length > 0) {
          setIsBotThinking(true);
          setGameState({ ...data, bot: { ...data.bot, bet: 0 } });
          await sleep(600);
          setIsBotThinking(false);
          setBotLastActionText(botActs[botActs.length - 1].action || null);
          sound.playChip();
        }

        setGameState(data);
        setHistoryEvents(data.events || []);

        if (data.match?.isMatchOver) {
          setIsMatchOverOpen(true);
        }
      } catch (err: unknown) {
        console.error("Error starting game:", err);
        setErrorMsg("Failed to start game with engine.");
      } finally {
        setIsPending(false);
      }
    },
    []
  );

  // Initial game start once bots loaded (starts in infinite mode)
  const initialStartedRef = useRef(false);
  useEffect(() => {
    if (bots.length > 0 && !initialStartedRef.current) {
      initialStartedRef.current = true;
      startNewGame(selectedBotId, selectedVersion, autoRebuy, "infinite");
    }
  }, [bots, selectedBotId, selectedVersion, autoRebuy, startNewGame]);

  // Handle Player Action
  const handleAction = async (action: "fold" | "check" | "call" | "raise", amount?: number) => {
    if (!gameState || isPending) return;

    setIsPending(true);
    setErrorMsg(null);

    // Immediate optimistic audio for hero
    if (action === "fold") sound.playFold();
    else if (action === "check") sound.playCheck();
    else if (amount && amount >= gameState.hero.stack + gameState.hero.bet) sound.playAllIn();
    else sound.playChip();

    try {
      const res = await fetch("/api/game/action", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ action, amount }),
      });
      if (!res.ok) {
        const err = await res.json();
        throw new Error(err.error || "Action failed");
      }
      const data: GameState = await res.json();

      // Append new events to hand history
      setHistoryEvents((prev) => [...prev, ...data.events]);

      // Playback sequence with delay
      await playbackEvents(data, data.events);
    } catch (err: unknown) {
      console.error("Error executing action:", err);
      setErrorMsg(err instanceof Error ? err.message : "Action failed");
    } finally {
      setIsBotThinking(false);
      setIsPending(false);
    }
  };

  // Handle Next Hand
  const handleNextHand = async () => {
    if (isPending) return;

    // In match mode, if match is already over, show the match results
    if (gameState?.match?.isMatchOver) {
      setIsMatchOverOpen(true);
      return;
    }

    setIsPending(true);
    setBotLastActionText(null);
    setErrorMsg(null);
    setRebuyAlert(null);
    displayedBoardCountRef.current = 0;
    try {
      const res = await fetch("/api/game/next", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ autoRebuy, gameMode }),
      });
      if (!res.ok) throw new Error("Failed to deal next hand");
      const data: GameState = await res.json();

      sound.playCardDeal();

      // Check for rebuy events (in infinite mode)
      const rebuyEvt = data.events.find((e) => e.type === "rebuy");
      if (rebuyEvt) {
        const pName = rebuyEvt.player === "hero" ? "You" : "Bot";
        setRebuyAlert(`${pName} was under 100 chips and automatically rebought 10,000 chips.`);
      }

      // Check if match mode reached terminal immediately (e.g. from all-in blind)
      if (data.match?.isMatchOver) {
        const winner = data.match.matchWinner;
        if (winner === "hero") {
          sound.playMatchVictory();
        } else {
          sound.playMatchDefeat();
        }
        setTimeout(() => {
          setIsMatchOverOpen(true);
        }, 1000);
      }

      // If bot acted first preflop, show pacing
      const botActs = data.events.filter((e) => e.actor === "bot");
      if (botActs.length > 0) {
        setIsBotThinking(true);
        setGameState({ ...data, bot: { ...data.bot, bet: 0 } });
        await sleep(600);
        setIsBotThinking(false);
        setBotLastActionText(botActs[botActs.length - 1].action || null);
        sound.playChip();
      }

      setGameState(data);
      setHistoryEvents((prev) => [...prev, ...data.events]);
    } catch (err: unknown) {
      console.error("Error dealing next hand:", err);
      setErrorMsg("Failed to deal next hand");
    } finally {
      setIsPending(false);
    }
  };

  // Handle Mode Selection from HeaderBar
  const handleSelectGameMode = (newMode: "infinite" | "match") => {
    if (newMode === "match") {
      // Show confirmation / ready popup first!
      setIsMatchReadyOpen(true);
    } else {
      setGameMode("infinite");
      setIsMatchOverOpen(false);
      startNewGame(selectedBotId, selectedVersion, autoRebuy, "infinite");
    }
  };

  // Confirm Start Match from MatchReadyModal
  const handleConfirmStartMatch = () => {
    setIsMatchReadyOpen(false);
    setIsMatchOverOpen(false);
    setHistoryEvents([]);
    startNewGame(selectedBotId, selectedVersion, false, "match");
  };

  // Cancel Match Mode from MatchReadyModal
  const handleCancelMatchReady = () => {
    setIsMatchReadyOpen(false);
  };

  // Play Again from MatchOverModal
  const handlePlayAgainMatch = () => {
    setIsMatchOverOpen(false);
    // Open the ready confirmation popup to confirm starting a new match
    setIsMatchReadyOpen(true);
  };

  // Play Infinite Mode from MatchOverModal
  const handlePlayInfiniteFromModal = () => {
    setIsMatchOverOpen(false);
    setGameMode("infinite");
    startNewGame(selectedBotId, selectedVersion, autoRebuy, "infinite");
  };

  // Handle Bot Selector change
  const handleSelectBot = (newBotId: string) => {
    setSelectedBotId(newBotId);
    if (gameMode === "match") {
      setIsMatchReadyOpen(true);
    } else {
      startNewGame(newBotId, selectedVersion, autoRebuy, "infinite");
    }
  };

  // Handle Hold'em Version Selector change
  const handleSelectVersion = (newVersion: string) => {
    setSelectedVersion(newVersion);
    if (gameMode === "match") {
      setIsMatchReadyOpen(true);
    } else {
      startNewGame(selectedBotId, newVersion, autoRebuy, "infinite");
    }
  };

  // Handle Auto Rebuy Stack Mode toggle
  const handleToggleAutoRebuy = async () => {
    const nextVal = !autoRebuy;
    setAutoRebuy(nextVal);
    try {
      await fetch("/api/game/toggle_rebuy", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ autoRebuy: nextVal }),
      });
    } catch (e) {
      console.error(e);
    }
  };

  // Reset Session
  const handleResetSession = () => {
    setHistoryEvents([]);
    if (gameMode === "match") {
      setIsMatchReadyOpen(true);
    } else {
      startNewGame(selectedBotId, selectedVersion, autoRebuy, "infinite");
    }
  };

  // Sound mute toggle
  const handleToggleMute = () => {
    const next = !isMuted;
    setIsMuted(next);
    sound.setMuted(next);
  };

  // BotCheck mode toggle
  const handleToggleBotCheck = () => {
    const next = !botCheckEnabled;
    setBotCheckEnabled(next);
    if (!next) {
      setIsBotCheckOpen(false);
    } else if (gameState?.isTerminal && gameState.bot?.actualHole) {
      setIsBotCheckOpen(true);
    }
  };

  const dummySession = {
    handsPlayed: 0,
    totalChipsWon: 0,
    bbPerHand: 0,
    bbPer100: 0,
    holdenDecisions: 0,
    holdenFallbacks: 0,
    holdenFallbackPct: 0,
    autoRebuy: false,
    heroBankroll: 10000,
    botBankroll: 10000,
  };

  const botName = gameState?.bot?.name || "Opponent";

  return (
    <div className="min-h-screen bg-zinc-950 text-zinc-100 flex flex-col justify-between font-sans selection:bg-emerald-500/30 selection:text-emerald-200">
      {/* Top Header */}
      <HeaderBar
        bots={bots}
        selectedBotId={selectedBotId}
        selectedVersion={selectedVersion}
        onSelectBot={handleSelectBot}
        onSelectVersion={handleSelectVersion}
        session={gameState?.session || dummySession}
        isMuted={isMuted}
        onToggleMute={handleToggleMute}
        fourColor={fourColor}
        onToggleFourColor={() => setFourColor((v) => !v)}
        onResetSession={handleResetSession}
        isPending={isPending}
        historyCount={historyEvents.length}
        onToggleHistory={() => setIsHistoryOpen((v) => !v)}
        isHistoryOpen={isHistoryOpen}
        botCheckEnabled={botCheckEnabled}
        onToggleBotCheck={handleToggleBotCheck}
        autoRebuy={autoRebuy}
        onToggleAutoRebuy={handleToggleAutoRebuy}
        gameMode={gameMode}
        onSelectGameMode={handleSelectGameMode}
        matchInfo={gameState?.match}
      />

      {/* Rebuy or error alerts */}
      {rebuyAlert && (
        <div className="bg-amber-950/90 border-b border-amber-700 text-amber-200 text-xs px-4 py-1.5 text-center font-mono font-semibold">
          ⚡ {rebuyAlert}
        </div>
      )}
      {errorMsg && (
        <div className="bg-red-950 border-b border-red-800 text-red-300 text-xs px-4 py-2 text-center font-mono">
          {errorMsg} - Run: <code className="bg-red-900/60 px-1.5 py-0.5 rounded">./build/poker_server 8085</code> in project root.
        </div>
      )}

      {/* Main Felt Canvas */}
      <main className="flex-1 flex flex-col items-center justify-center py-2 sm:py-4 relative">
        <PokerTable
          gameState={gameState}
          fourColor={fourColor}
          botLastActionText={botLastActionText}
          isBotThinking={isBotThinking}
        />

        {/* BotCheck Mode Side Window */}
        <BotCheckPanel
          botCheckEnabled={botCheckEnabled}
          onToggleBotCheck={handleToggleBotCheck}
          botActualHole={gameState?.bot?.actualHole || null}
          botActualRank={gameState?.bot?.actualHandRank}
          foldedPlayer={gameState?.bot?.foldedPlayer}
          street={gameState?.street}
          isTerminal={gameState?.isTerminal || false}
          isOpen={isBotCheckOpen}
          onClose={() => setIsBotCheckOpen(false)}
          botName={botName}
          fourColor={fourColor}
        />
      </main>

      {/* Action Controls Dock with 30-Second Shot Clock */}
      <footer className="w-full bg-zinc-950/95 border-t border-zinc-850 py-1">
        {gameState && (
          <BettingControls
            key={gameState.handNumber}
            legalActions={gameState.legalActions || []}
            toAct={gameState.toAct}
            isTerminal={gameState.isTerminal}
            toCall={gameState.toCall}
            pot={gameState.pot}
            heroStack={gameState.hero.stack}
            heroBet={gameState.hero.bet}
            onAction={handleAction}
            onNextHand={handleNextHand}
            isPending={isPending}
            street={gameState.street}
            isMatchOver={gameState.match?.isMatchOver}
            onOpenMatchOver={() => setIsMatchOverOpen(true)}
          />
        )}
      </footer>

      {/* Slide-Over History Log Modal */}
      <ActionHistoryLog
        events={historyEvents}
        isOpen={isHistoryOpen}
        onClose={() => setIsHistoryOpen(false)}
      />

      {/* Match Mode: Pre-match "Ready?" Modal */}
      <MatchReadyModal
        isOpen={isMatchReadyOpen}
        botName={botName}
        onConfirmStart={handleConfirmStartMatch}
        onCancelToInfinite={handleCancelMatchReady}
      />

      {/* Match Mode: Post-match "Match Over" Modal */}
      <MatchOverModal
        isOpen={isMatchOverOpen}
        winner={gameState?.match?.matchWinner || "none"}
        botName={botName}
        handsPlayed={gameState?.match?.matchHandNumber || gameState?.session?.handsPlayed || 0}
        heroBankroll={gameState?.session?.heroBankroll || 0}
        botBankroll={gameState?.session?.botBankroll || 0}
        onPlayAgain={handlePlayAgainMatch}
        onPlayInfinite={handlePlayInfiniteFromModal}
      />
    </div>
  );
}
