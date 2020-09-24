
#include "PPipeGrid_PlaceWorld.h"
#include "PPipeGrid_Internal.h"

void APPipeGrid::EnablePlaceWorld(bool enabled)
{
    m_placeWorldState =
    {
        /* Mode */              enabled ? PlaceWorldMode::Pending : PlaceWorldMode::None,
        /* LeftActive */        false,
        /* RightActive */       false,
        /* LeftPosition */      FVector::ZeroVector,
        /* RightPosition */     FVector::ZeroVector,

        /* OneHand */           {
        /* AttachedToHand */        EFingerHand::Count,
        /* AttachToGame */          FTransform(),
                                },

        /* TwoHand */           {
        /* StartScale */            FVector::ZeroVector,
        /* StartGridLocation */     FVector::ZeroVector,
        /* StartHeadToHand */       FVector::ZeroVector,
        /* PreviousLeft */          FVector::ZeroVector,
        /* PreviousRight */         FVector::ZeroVector,
        /* StartDistance */         0
                                }
    };

    ChangeInteractionEnabledForHand(m_hands.Left);
    ChangeInteractionEnabledForHand(m_hands.Right);
}

bool APPipeGrid::GetPlaceWorldEnabled()
{
    return m_placeWorldState.Mode != PlaceWorldMode::None;
}

void APPipeGrid::HandlePlaceWorldGestureUpdate(const FGestureUpdate& update)
{
    m_placeWorldState.LeftActive = (update.Left.Progress == EGestureProgress::Started || update.Left.Progress == EGestureProgress::Updated);
    m_placeWorldState.LeftPosition = update.Left.Target.GetLocation();

    m_placeWorldState.RightActive = (update.Right.Progress == EGestureProgress::Started || update.Right.Progress == EGestureProgress::Updated);
    m_placeWorldState.RightPosition = update.Right.Target.GetLocation();

    PlaceWorldMode nextMode = PlaceWorldMode::Pending;
    if (m_placeWorldState.LeftActive != m_placeWorldState.RightActive)
    {
        nextMode = PlaceWorldMode::OneHand;
    }
    else if (m_placeWorldState.LeftActive)
    {
        nextMode = PlaceWorldMode::TwoHand;
    }

    if (nextMode != PlaceWorldMode::OneHand)
    {
        m_placeWorldState.OneHand.AttachedToHand = EFingerHand::Count;
    }

    switch (nextMode)
    {
        case PlaceWorldMode::Pending:
            m_placeWorldState.Mode = PlaceWorldMode::Pending;
            break;

        case PlaceWorldMode::OneHand:
        {
            const FTransform& attachTransform = m_placeWorldState.LeftActive ? update.Left.Attach : update.Right.Attach;

            const FTransform stableAttachToWorld = FTransform(UKismetMathLibrary::MakeRotFromZX(FVector::UpVector, attachTransform.GetLocation() - Pawn->VRCamera->GetComponentLocation()),
                                                              attachTransform.GetLocation());

            bool attachedToActive =
                (m_placeWorldState.LeftActive && m_placeWorldState.OneHand.AttachedToHand == EFingerHand::Left) ||
                (m_placeWorldState.RightActive && m_placeWorldState.OneHand.AttachedToHand == EFingerHand::Right);

            if (m_placeWorldState.Mode != PlaceWorldMode::OneHand || !attachedToActive)
            {
                const FTransform gameToWorld = GetActorTransform();

                const FTransform stableWorldToAttach = stableAttachToWorld.Inverse();

                m_placeWorldState.OneHand.GameToAttach = gameToWorld * stableWorldToAttach;

                if (m_placeWorldState.OneHand.GameToAttach.IsValid())
                {
                    m_placeWorldState.Mode = PlaceWorldMode::OneHand;
                    m_placeWorldState.OneHand.AttachedToHand = (m_placeWorldState.LeftActive ? EFingerHand::Left : EFingerHand::Right);
                }
            }
            else
            {
                FTransform gameToWorld = m_placeWorldState.OneHand.GameToAttach * stableAttachToWorld;

                if (gameToWorld.IsValid())
                {
                    SetActorTransform(gameToWorld);
                }
            }
            break;
        }

        case PlaceWorldMode::TwoHand:

            const float currentDistance = FVector::Distance(m_placeWorldState.LeftPosition, m_placeWorldState.RightPosition);
            const FVector currentHandWorldLocation = (m_placeWorldState.LeftPosition + m_placeWorldState.RightPosition) / 2;
            const FVector currentHeadToHand = (currentHandWorldLocation - Pawn->VRCamera->GetComponentLocation());

            if (m_placeWorldState.Mode != PlaceWorldMode::TwoHand)
            {
                m_placeWorldState.Mode = PlaceWorldMode::TwoHand;
                m_placeWorldState.TwoHand.StartDistance = currentDistance;
                m_placeWorldState.TwoHand.StartGridWorldOffset = this->GetActorLocation() - currentHandWorldLocation;
                m_placeWorldState.TwoHand.StartHeadToHand = currentHeadToHand;
                m_placeWorldState.TwoHand.StartScale = GetActorScale();
            }
            else
            {
                if (currentDistance > 0 && m_placeWorldState.TwoHand.StartDistance > 0)
                {
                    float ratio = currentDistance / m_placeWorldState.TwoHand.StartDistance;

                    // Assume that we only scale proportionally
                    float newScale = ratio * m_placeWorldState.TwoHand.StartScale.X;
                    float clampedScale = std::max(MinGridScale, std::min(MaxGridScale, newScale));

                    if (newScale != clampedScale)
                    {
                        ratio = clampedScale / m_placeWorldState.TwoHand.StartScale.X;
                    }

                    this->SetActorScale3D(ratio * m_placeWorldState.TwoHand.StartScale);

                    FVector previousHandVector2D = m_placeWorldState.TwoHand.PreviousRight - m_placeWorldState.TwoHand.PreviousLeft;
                    previousHandVector2D.Z = 0;

                    FVector currentHandVector2D = (m_placeWorldState.RightPosition - m_placeWorldState.LeftPosition);
                    currentHandVector2D.Z = 0;

                    this->AddActorWorldRotation(FQuat::FindBetweenVectors(previousHandVector2D, currentHandVector2D));

                    FVector previousHeadToHand2D = m_placeWorldState.TwoHand.StartHeadToHand;
                    previousHeadToHand2D.Z = 0;

                    FVector currentHeadToHand2D = currentHeadToHand;
                    currentHeadToHand2D.Z = 0;

                    const FQuat gridWorldOffsetRotation = FQuat::FindBetweenVectors(previousHeadToHand2D, currentHeadToHand2D);
                    const FVector currentGridWorldOffset = gridWorldOffsetRotation.RotateVector(ratio * m_placeWorldState.TwoHand.StartGridWorldOffset);

                    this->SetActorLocation(currentHandWorldLocation + currentGridWorldOffset);
                }
            }

            m_placeWorldState.TwoHand.PreviousLeft = m_placeWorldState.LeftPosition;
            m_placeWorldState.TwoHand.PreviousRight = m_placeWorldState.RightPosition;

            break;
    }
}