import { NextRequest, NextResponse } from "next/server";

const BACKEND_URL = process.env.POKER_BACKEND_URL || "http://127.0.0.1:8085";

export async function GET(
  req: NextRequest,
  context: { params: Promise<{ path: string[] }> }
) {
  const { path } = await context.params;
  const endpoint = "/api/" + path.join("/");
  try {
    const res = await fetch(`${BACKEND_URL}${endpoint}`, {
      method: "GET",
      headers: { "Content-Type": "application/json" },
      cache: "no-store",
    });
    const data = await res.json();
    return NextResponse.json(data, { status: res.status });
  } catch (err: unknown) {
    const errorMsg = err instanceof Error ? err.message : String(err);
    return NextResponse.json(
      {
        error: "Poker C++ server unreachable",
        details: errorMsg,
        tip: "Ensure ./build/poker_server is running on port 8085",
      },
      { status: 503 }
    );
  }
}

export async function POST(
  req: NextRequest,
  context: { params: Promise<{ path: string[] }> }
) {
  const { path } = await context.params;
  const endpoint = "/api/" + path.join("/");
  try {
    const bodyText = await req.text();
    const res = await fetch(`${BACKEND_URL}${endpoint}`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: bodyText,
      cache: "no-store",
    });
    const data = await res.json();
    return NextResponse.json(data, { status: res.status });
  } catch (err: unknown) {
    const errorMsg = err instanceof Error ? err.message : String(err);
    return NextResponse.json(
      {
        error: "Poker C++ server unreachable",
        details: errorMsg,
        tip: "Ensure ./build/poker_server is running on port 8085",
      },
      { status: 503 }
    );
  }
}
