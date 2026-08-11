/** @type {import('next').NextConfig} */
const nextConfig = {
  typescript: {
    ignoreBuildErrors: true,
  },
  images: {
    unoptimized: true,
  },
  turbopack: {
    root: process.cwd(),
  },
  async rewrites() {
    return [
      { source: "/nova", destination: "/nova/index.html" },
      { source: "/nova/", destination: "/nova/index.html" },
    ];
  },
}

export default nextConfig
