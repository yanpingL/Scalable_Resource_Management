import { afterEach, describe, expect, it, vi } from "vitest";
import axios from "axios";
import { apiFetch } from "@/shared/api/client";

vi.mock("axios");

const mockedAxios = vi.mocked(axios);

describe("apiFetch", () => {
  afterEach(() => {
    vi.clearAllMocks();
  });

  it("returns parsed JSON for successful responses", async () => {
    mockedAxios.request.mockResolvedValue({
      data: { status: "created" },
    });

    await expect(apiFetch("/api/register")).resolves.toEqual({
      status: "created",
    });
  });

  it("throws backend error messages from JSON error bodies", async () => {
    mockedAxios.isAxiosError.mockReturnValue(true);
    mockedAxios.request.mockRejectedValue({
      response: {
        data: { error: "wrong password" },
        status: 400,
      },
    });

    await expect(apiFetch("/api/login")).rejects.toMatchObject({
      message: "wrong password",
      status: 400,
    });
  });
});
