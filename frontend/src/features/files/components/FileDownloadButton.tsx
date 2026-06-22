"use client";

import { useMutation } from "@tanstack/react-query";
import { requestDownloadUrl } from "../api";
import type { DownloadUrlResponse } from "../types";

type FileDownloadButtonProps = {
  className?: string;
  fileName?: string;
  label?: string;
  resourceId: number;
};

function fallbackFileName(data: DownloadUrlResponse) {
  const objectKeyName = data.object_key.split("/").pop();
  const publicUrlName = data.public_url.split("/").pop()?.split("?")[0];

  return objectKeyName || publicUrlName || "download";
}

function safeFileName(fileName: string) {
  const normalized = fileName.trim().replace(/[\\/:*?"<>|]+/g, "-");

  return normalized || "download";
}

function hasFileExtension(fileName: string) {
  return /\.[A-Za-z0-9]{1,12}$/.test(fileName);
}

function fileExtension(fileName: string) {
  const match = fileName.match(/(\.[A-Za-z0-9]{1,12})$/);

  return match?.[1] ?? "";
}

function downloadFileName(data: DownloadUrlResponse, preferredFileName?: string) {
  const fallback = fallbackFileName(data);
  const preferred = preferredFileName?.trim();

  if (!preferred) {
    return fallback;
  }

  if (hasFileExtension(preferred)) {
    return preferred;
  }

  return `${preferred}${fileExtension(fallback)}`;
}

async function downloadFromSignedUrl(
  data: DownloadUrlResponse,
  preferredFileName?: string,
) {
  let objectUrl: string | null = null;

  try {
    const response = await fetch(data.download_url);

    if (!response.ok) {
      throw new Error(`Download failed with status ${response.status}`);
    }

    const blob = await response.blob();
    objectUrl = URL.createObjectURL(blob);
    const link = document.createElement("a");

    link.href = objectUrl;
    link.download = safeFileName(downloadFileName(data, preferredFileName));
    document.body.appendChild(link);
    link.click();
    link.remove();
  } catch (error) {
    throw new Error(
      error instanceof Error ? error.message : "Download failed.",
    );
  } finally {
    if (objectUrl) {
      URL.revokeObjectURL(objectUrl);
    }
  }
}

export function FileDownloadButton({
  className,
  fileName,
  label = "Download",
  resourceId,
}: FileDownloadButtonProps) {
  // First ask the backend for a short-lived download URL for this resource.
  const downloadMutation = useMutation({
    mutationFn: async (id: number) => {
      const data = await requestDownloadUrl(id);

      await downloadFromSignedUrl(data, fileName);
    },
  });

  return (
    <div className="space-y-2">
      <button
        className={
          className ??
          "rounded-md border border-slate-300 px-3 py-2 text-sm font-semibold transition hover:-translate-y-0.5 hover:bg-slate-100 disabled:translate-y-0 disabled:opacity-50"
        }
        disabled={downloadMutation.isPending}
        onClick={(event) => {
          event.stopPropagation();
          downloadMutation.mutate(resourceId);
        }}
        type="button"
      >
        {downloadMutation.isPending ? "Preparing..." : label}
      </button>
      {downloadMutation.isError ? (
        <p className="text-sm text-red-700">{downloadMutation.error.message}</p>
      ) : null}
    </div>
  );
}
