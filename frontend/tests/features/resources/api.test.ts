import { afterEach, beforeEach, describe, expect, it, vi } from "vitest";
import axios from "axios";
import {
  createFileResource,
  createTextResource,
  deleteResource,
  getResource,
  listResources,
  updateTextResource,
} from "@/features/resources/api";

vi.mock("axios");

const mockedAxios = vi.mocked(axios);

describe("resources api", () => {
  beforeEach(() => {
    window.localStorage.setItem("resource_manager_token", "jwt-token");
  });

  afterEach(() => {
    window.localStorage.clear();
    vi.clearAllMocks();
  });

  it("lists resources with the bearer token", async () => {
    mockedAxios.request.mockResolvedValue({ data: { data: [] } });

    await expect(listResources()).resolves.toEqual([]);

    expect(mockedAxios.request).toHaveBeenCalledWith({
      url: "/api/resources",
      method: "GET",
      headers: {
        Authorization: "Bearer jwt-token",
        "Content-Type": "application/json",
      },
    });
  });

  it("gets a single resource by id", async () => {
    mockedAxios.request.mockResolvedValue({
      data: {
        id: 7,
        title: "One",
        content: "Body",
        is_file: false,
      },
    });

    await getResource(7);

    expect(mockedAxios.request).toHaveBeenCalledWith({
      url: "/api/resources?id=7",
      method: "GET",
      headers: {
        Authorization: "Bearer jwt-token",
        "Content-Type": "application/json",
      },
    });
  });

  it("creates, updates, and deletes resources with expected payloads", async () => {
    mockedAxios.request.mockResolvedValue({ data: { status: "created" } });

    await createTextResource({ title: "Text", content: "Hello" });
    await createFileResource({ title: "File", content: "http://file" });
    await updateTextResource(3, { title: "Updated", content: "Body" });
    await deleteResource(3);

    expect(mockedAxios.request).toHaveBeenNthCalledWith(
      1,
      expect.objectContaining({
        url: "/api/resources",
        method: "POST",
        data: JSON.stringify({
          title: "Text",
          content: "Hello",
          is_file: false,
        }),
      }),
    );
    expect(mockedAxios.request).toHaveBeenNthCalledWith(
      2,
      expect.objectContaining({
        url: "/api/resources",
        method: "POST",
        data: JSON.stringify({
          title: "File",
          content: "http://file",
          is_file: true,
        }),
      }),
    );
    expect(mockedAxios.request).toHaveBeenNthCalledWith(
      3,
      expect.objectContaining({
        url: "/api/resources",
        method: "PUT",
        data: JSON.stringify({
          id: 3,
          title: "Updated",
          content: "Body",
        }),
      }),
    );
    expect(mockedAxios.request).toHaveBeenNthCalledWith(
      4,
      expect.objectContaining({
        url: "/api/resources?id=3",
        method: "DELETE",
      }),
    );
  });
});
