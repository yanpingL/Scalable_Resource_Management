FROM node:24.9.0-bookworm-slim AS deps

WORKDIR /app

COPY package.json package-lock.json ./
RUN npm ci

FROM node:24.9.0-bookworm-slim AS builder

WORKDIR /app

ARG API_BASE_URL=http://nginx
ENV API_BASE_URL=${API_BASE_URL}
ENV NEXT_TELEMETRY_DISABLED=1

COPY --from=deps /app/node_modules ./node_modules
COPY . .
RUN npm run build

FROM node:24.9.0-bookworm-slim AS runner

WORKDIR /app

ENV NODE_ENV=production
ENV API_BASE_URL=http://nginx
ENV NEXT_TELEMETRY_DISABLED=1
ENV HOSTNAME=0.0.0.0
ENV PORT=3000

COPY package.json package-lock.json ./
RUN npm ci --omit=dev && npm cache clean --force

COPY --from=builder /app/.next ./.next
COPY --from=builder /app/public ./public
COPY --from=builder /app/next.config.ts ./next.config.ts

EXPOSE 3000

CMD ["npm", "run", "start"]
