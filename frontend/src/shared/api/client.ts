import axios, { type AxiosRequestConfig } from "axios";

export class ApiError extends Error {
  constructor(
    message: string,
    readonly status: number,
  ) {
    super(message);
    this.name = "ApiError";
  }
}

function messageFromUnknownError(error: unknown) {
  if (error instanceof Error && error.message) {
    return error.message;
  }

  return "Request failed. Please try again.";
}

// Shared HTTP wrapper: feature APIs call this instead of using HTTP clients directly.
export async function apiFetch<T>(
  path: string,
  options: RequestInit = {},
): Promise<T> {
  const isFormData = options.body instanceof FormData;
  const config: AxiosRequestConfig = {
    url: path,
    method: options.method ?? "GET",
    headers: {
      ...(isFormData ? {} : { "Content-Type": "application/json" }),
      ...(options.headers as Record<string, string> | undefined),
    },
    data: options.body,
  };

  try {
    const response = await axios.request<T>(config);
    return response.data;
  } catch (error) {
    if (axios.isAxiosError(error) && error.response) {
      const body = error.response.data;
      // The C++ backend returns JSON errors as { "error": "..." }.
      const message =
        body &&
        typeof body === "object" &&
        "error" in body &&
        typeof body.error === "string"
          ? body.error
          : `Request failed with status ${error.response.status}`;

      throw new ApiError(message, error.response.status);
    }

    if (axios.isAxiosError(error)) {
      throw new ApiError("Network request failed. Please try again.", 0);
    }

    throw new ApiError(messageFromUnknownError(error), 0);
  }
}
