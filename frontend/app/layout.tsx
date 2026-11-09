import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Hold'em 1v1 | Bot Arena",
  description: "Heads-up No-Limit Texas Hold'em against RuleBot, EquityBot, and Hold'em CFR Bot",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="en" className="h-full antialiased dark">
      <body className="min-h-full flex flex-col bg-zinc-950 text-zinc-100 font-sans selection:bg-emerald-500/30 selection:text-emerald-200">
        {children}
      </body>
    </html>
  );
}
