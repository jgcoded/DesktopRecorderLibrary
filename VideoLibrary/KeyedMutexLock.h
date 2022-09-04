/*
    Copyright (C) 2022 by Julio Gutierrez (desktoprecorderapp@gmail.com)

    This file is part of DesktopRecorderLibrary.

    DesktopRecorderLibrary is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by the
    Free Software Foundation, either version 3 of the License,
    or (at your option) any later version.

    DesktopRecorderLibrary is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DesktopRecorderLibrary. If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

class RotatingKeys
{
public:

    RotatingKeys()
        : RotatingKeys(0, 1)
    {
    }

    RotatingKeys(int acquireKey, int releaseKey)
        : mAcquireKey{ acquireKey }
        , mReleaseKey{ releaseKey }
    {
    }

    int AcquireKey() const
    {
        return mAcquireKey;
    }

    int ReleaseKey() const
    {
        return mReleaseKey;
    }

    void Rotate()
    {
        auto previousRelease = mReleaseKey;
        mReleaseKey = mAcquireKey;
        mAcquireKey = previousRelease;
    }

private:
    int mAcquireKey;
    int mReleaseKey;
};

class KeyedMutexLock
{
public:
    KeyedMutexLock(winrt::com_ptr<IDXGIKeyedMutex> mutex, std::shared_ptr<RotatingKeys> rotatingKeys)
        : mMutex{ mutex }
        , mRotatingKeys{ rotatingKeys }
        , mLocked{ false }
    {
        if (mMutex == nullptr)
        {
            throw std::exception("null mutex");
        }

        if (mRotatingKeys == nullptr)
        {
            throw std::exception("null rotating keys keyed mutex lock");
        }

        HRESULT hr = mutex->AcquireSync(mRotatingKeys->AcquireKey(), 10);
        if (hr == static_cast<HRESULT>(WAIT_TIMEOUT))
        {
            return;
        }

        winrt::check_hresult(hr);
        mLocked = true;
    }

    ~KeyedMutexLock()
    {
        auto releaseKey = mRotatingKeys->ReleaseKey();
        mRotatingKeys->Rotate();
        winrt::check_hresult(mMutex->ReleaseSync(releaseKey));
    }

    bool Locked() const { return mLocked; }

private:
    winrt::com_ptr<IDXGIKeyedMutex> mMutex;
    bool mLocked;
    std::shared_ptr<RotatingKeys> mRotatingKeys;
};
