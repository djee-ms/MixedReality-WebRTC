// Copyright (c) Microsoft Corporation.
// Licensed under the MIT License.

package com.microsoft.mixedreality.webrtc;

import android.content.Context;
import org.webrtc.NativeLibraryLoader;
import org.webrtc.PeerConnectionFactory;
import org.webrtc.PeerConnectionFactory.InitializationOptions;

public final class LibraryInitializationHelper {
    private static class NoopLibraryLoader implements NativeLibraryLoader {
        @Override
        public boolean load(String name) {
            return true;
        }
    }

    public static void InitializeLibrary(Context ctx) {
        InitializationOptions options =
                InitializationOptions.builder(ctx)
                        //.setNativeLibraryLoader(new NoopLibraryLoader())
                        .createInitializationOptions();

        PeerConnectionFactory.initialize(options);
    }
}
