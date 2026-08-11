import React from "react"
import type { Metadata, Viewport } from "next";
import { Rajdhani, JetBrains_Mono } from "next/font/google";

import "./globals.css";

const _rajdhani = Rajdhani({
  subsets: ["latin"],
  weight: ["300", "400", "500", "600", "700"],
  variable: "--font-rajdhani",
});

const _jetbrainsMono = JetBrains_Mono({
  subsets: ["latin"],
  variable: "--font-jetbrains",
});

export const metadata: Metadata = {
  title: "SAVE THE WORLD – Rette die Welt auf deine Art",
  description:
    "First-Person-Shooter mit eigener WebGL-Engine (STW-ENGINE): Destruction, Gunplay, 6 Multiplayer-Modi, Story-Kampagne – spielbar im Browser & auf dem Handy.",
  generator: "v0.app",
};

export const viewport: Viewport = {
  width: "device-width",
  initialScale: 1,
  maximumScale: 1,
  userScalable: false,
  viewportFit: "cover",
};

export default function RootLayout({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <html lang="de" className="bg-background">
      <head>
        <link rel="manifest" href="/manifest.json" />
        <meta name="mobile-web-app-capable" content="yes" />
        <meta name="apple-mobile-web-app-capable" content="yes" />
        <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent" />
        <meta name="apple-mobile-web-app-title" content="STW" />
        <link rel="icon" href="/icons/icon-512.png" />
        <link rel="apple-touch-icon" href="/icons/icon-512.png" />
      </head>
      <body className="font-sans antialiased">
        {children}
        <script
          dangerouslySetInnerHTML={{
            __html:
              "if ('serviceWorker' in navigator) { window.addEventListener('load', function () { navigator.serviceWorker.register('/sw.js').catch(function () {}); }); }",
          }}
        />
      </body>
    </html>
  );
}
